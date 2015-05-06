/* ts=4 */
/*
** Copyright 2011 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>
#include "Xsocket.h"
#include "dagaddr.hpp"
#include <assert.h>

#include <sys/time.h>

#include "prefetch_utils.h"

#define MAX_XID_SIZE 100
#define VERSION "v1.0"
#define TITLE "XIA Advanced FTP client"
#define CHUNKSIZE 1024
#define REREQUEST 3

#define NUM_CHUNKS	10
#define NUM_PROMPTS	2

/*
MAXBUFLEN = XIA_MAXBUF = XIA_MAXCHUNK = 15600
*/

// global configuration options
bool quick = false;

char s_ad[MAX_XID_SIZE];
char s_hid[MAX_XID_SIZE];

char my_ad[MAX_XID_SIZE];
char my_hid[MAX_XID_SIZE];

char* ftp_name = "www_s.ftp.advanced.aaa.xia";
char* prefetch_client_name = "www_s.client.prefetch.aaa.xia";
char* prefetch_profile_name = "www_s.profile.prefetch..aaa.xia";
char* prefetch_pred_name = "www_s.prediction.prefetch.aaa.xia";
char* prefetch_exec_name = "www_s.executer.prefetch.aaa.xia";

char* prefetch_ctx_name = "www_s.prefetch_context_listener.aaa.xia";

int ftp_sock = -1;
int	prefetch_ctx_sock = -1;

int getFile(int sock, char *p_ad, char* p_hid, const char *fin, const char *fout);

void *recvCmd (void *socketid) {
	int i, n, count = 0;
	ChunkInfo *info = NULL;
	char command[XIA_MAXBUF];
	char reply[XIA_MAXBUF];
	int sock = *((int*)socketid);
	char *fname;
	
	//ChunkContext contains size, ttl, policy, and contextID which for now is PID
	ChunkContext *ctx = XallocCacheSlice(POLICY_FIFO|POLICY_REMOVE_ON_EXIT, 0, 20000000);
	if (ctx == NULL)
		die(-2, "Unable to initilize the chunking system\n");

	while (1) {
		say("waiting for server command\n");
		memset(command, '\0', strlen(command));
		memset(reply, '\0', strlen(reply));
		if ((n = Xrecv(sock, command, 1024, 0))  < 0) {
			warn("socket error while waiting for data, closing connection\n");
			break;
		}
		// Sender does the chunking and then should start the sending commands
		if (strncmp(command, "get", 3) == 0) {
			fname = &command[4];
			say("Server requested file %s\n", fname);

			if (info) {
				// clean up the existing chunks first
			}
		
			info = NULL;
			count = 0;
			
			say("chunking file %s\n", fname);
			
			// Chunking is done by the XputFile which itself uses XputChunk, and fills out the info
			if ((count = XputFile(ctx, fname, CHUNKSIZE, &info)) < 0) {
				warn("unable to serve the file: %s\n", fname);
				sprintf(reply, "FAIL: File (%s) not found", fname);

			} else {
				sprintf(reply, "OK: %d", count);
			}
			say("%s\n", reply);
			
			// Just tells the receiver how many chunks it should expect in total.
			if (Xsend(sock, reply, strlen(reply), 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}
		// Sending the NUM_CHUNKS of cids
		} else if (strncmp(command, "block", 5) == 0) {
			char *start = &command[6];
			char *end = strchr(start, ':');
			if (!end) {
				// we have an invalid command, return error to client
				sprintf(reply, "FAIL: invalid block command");
			} else {
				*end = 0;
				end++;
				// FIXME: clean up naming, e is now a count
				int s = atoi(start);
				int e = s + atoi(end);
				strcpy(reply, "OK:");
				for(i = s; i < e && i < count; i++) {
					strcat(reply, " ");
					strcat(reply, info[i].cid);
				}
			}
			printf("%s\n", reply);
			if (Xsend(sock, reply, strlen(reply), 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}

		} else if (strncmp(command, "done", 4) == 0) {
			say("done sending file: removing the chunks from the cache\n");
			for (int i = 0; i < count; i++)
				XremoveChunk(ctx, info[i].cid);
			XfreeChunkInfo(info);
			info = NULL;
			count = 0;
			break;		
		}
		else {
			sprintf(reply, "FAIL: invalid command (%s)\n", command);
			warn(reply);
			if (Xsend(sock, reply, strlen(reply), 0) < 0) {
				warn("unable to send reply to client\n");
				break;
			}
		}
	}
	
	if (info)
		XfreeChunkInfo(info);
	XfreeCacheSlice(ctx);
	return (void *)1;
}

void *blockingListener(void *socketid) {
  int sock = *((int*)socketid);
  int acceptSock;
  while (1) {
		say("Waiting for a client connection\n");
   		
		if ((acceptSock = Xaccept(sock, NULL, NULL)) < 0)
			die(-1, "accept failed\n");

		say("connected\n");
		
		// handle the connection in a new thread
		pthread_t client;
		pthread_create(&client, NULL, recvCmd, (void *)&acceptSock);
	}
	
	Xclose(sock); // we should never reach here!
	return NULL;
}

// not used
void nonblockingListener(int sock) {
	pthread_t client;
  pthread_create(&client, NULL, blockingListener, (void *)&sock);
}

int getChunkCount(int sock, char *reply, int sz) {
	int n = -1;

	if ((n = Xrecv(sock, reply, sz, 0))  < 0) {
		Xclose(sock);
		 die(-1, "Unable to communicate with the server\n");
	}

	if (strncmp(reply, "OK:", 3) != 0) {
		warn( "%s\n", reply);
		return -1;
	}

	reply[n] = 0;

	return n;
}

int buildChunkDAGs(ChunkStatus cs[], char *chunks, char *p_ad, char *p_hid) {
	char *p = chunks;
	char *next;
	int n = 0;

	char *dag;
	
	// build the list of chunks to retrieve
	while ((next = strchr(p, ' '))) {
		*next = 0;
		dag = (char *)malloc(512);
		sprintf(dag, "RE ( %s %s ) CID:%s", p_ad, p_hid, p);
		// printf("built dag: %s\n", dag);
		cs[n].cidLen = strlen(dag);
		cs[n].cid = dag;
		n++;
		p = next + 1;
	}
	dag = (char *)malloc(512);
	sprintf(dag, "RE ( %s %s ) CID:%s", p_ad, p_hid, p);
	// printf("getting %s\n", p);
	cs[n].cidLen = strlen(dag);
	cs[n].cid = dag;
	n++;
	return n;
}

int getListedChunks(int csock, FILE *fd, char *chunks, char *p_ad, char *p_hid) {
	ChunkStatus cs[NUM_CHUNKS];
	char data[XIA_MAXCHUNK];
	int len;
	int status;
	int n = -1;
	
	n = buildChunkDAGs(cs, chunks, p_ad, p_hid);
	
	// NOTE: the chunk transport is not currently reliable and chunks may need to be re-requested
	// ask for the current chunk list again every REREQUEST seconds
	// chunks already in the local cache will not be refetched from the network 
	// read the the whole chunk list first before fetching
	unsigned ctr = 0;
	while (1) {
		if (ctr % REREQUEST == 0) {
			// bring the list of chunks local
			say("%srequesting list of %d chunks\n", (ctr == 0 ? "" : "re-"), n);
			if (XrequestChunks(csock, cs, n) < 0) {
				say("unable to request chunks\n");
				return -1;
			}
			say("checking chunk status\n");
		}
		ctr++;

		status = XgetChunkStatuses(csock, cs, n);

		if (status == READY_TO_READ)
			break;

		else if (status < 0) {
			say("error getting chunk status\n");
			return -1;

		} else if (status & WAITING_FOR_CHUNK) {
			// one or more chunks aren't ready.
			say("waiting... one or more chunks aren't ready yet\n");
		
		} else if (status & INVALID_HASH) {
			die(-1, "one or more chunks has an invalid hash");
		
		} else if (status & REQUEST_FAILED) {
			die(-1, "no chunks found\n");

		} else {
			say("unexpected result\n");
		}
		sleep(1);
	}

	say("all chunks ready\n");

	for (int i = 0; i < n; i++) {
		char *cid = strrchr(cs[i].cid, ':');
		cid++;
		say("reading chunk %s\n", cid);
		if ((len = XreadChunk(csock, data, sizeof(data), 0, cs[i].cid, cs[i].cidLen)) < 0) {
			say("error getting chunk\n");
			return -1;
		}

		// write the chunk to disk
		// say("writing %d bytes of chunk %s to disk\n", len, cid);
		fwrite(data, 1, len, fd);

		free(cs[i].cid);
		cs[i].cid = NULL;
		cs[i].cidLen = 0;
	}

	return n;
}

//	This is used both to put files and to get files since in case of put I still have to request the file.
//	Should be fixed with push implementation
int getFile(int sock, char *p_ad, char* p_hid, const char *fin, const char *fout) {
	int chunkSock;
	int offset;
	char cmd[512];
	char reply[512];
	int status = 0;
	char prefetch_cmd[512];

	// send the file request to the xftp server
	sprintf(cmd, "get %s",  fin);
	sendCmd(sock, cmd);

	// get back number of chunks in the file
	if (getChunkCount(sock, reply, sizeof(reply)) < 1){
		warn("could not get chunk count. Aborting. \n");
		return -1;
	}

	// reply: OK: ***
	int count = atoi(&reply[4]);

	say("%d chunks in total\n", count);

	if ((chunkSock = Xsocket(AF_XIA, XSOCK_CHUNK, 0)) < 0)
		die(-1, "unable to create chunk socket\n");

	FILE *f = fopen(fout, "w");

	offset = 0;

	struct timeval tv;
	int start_msec, temp_start_msec, temp_end_msec;
	
	if (gettimeofday(&tv, NULL) == 0)
		start_msec = ((tv.tv_sec % 86400) * 1000 + tv.tv_usec / 1000);
			
	while (offset < count) {
		int num = NUM_CHUNKS;
		if (count - offset < num)
			num = count - offset;

		// tell the server we want a list of <num> cids starting at location <offset>
		printf("\nFetched chunks: %d/%d:%.1f%\n\n", offset, count, 100*(double)(offset)/count);

		sprintf(cmd, "block %d:%d", offset, num);
		
		if (gettimeofday(&tv, NULL) == 0)
			temp_start_msec = ((tv.tv_sec % 86400) * 1000 + tv.tv_usec / 1000);
					
		// send the requested CID range
		sendCmd(sock, cmd);

		// TODO: send file name and current chunks 
		sprintf(prefetch_cmd, "get %s %d %d", fin, offset, offset + NUM_CHUNKS - 1);
		int m = sendCmd(prefetch_ctx_sock, prefetch_cmd); 
/*
		char* hello = "Hello from context client";
		int m = sendCmd(prefetch_ctx_sock, hello); 
		printf("%d\n");
		say("Sent CID update\n");
*/
		if (getChunkCount(sock, reply, sizeof(reply)) < 1) {
			warn("could not get chunk count. Aborting. \n");
			return -1;
		}
		offset += NUM_CHUNKS;
		// &reply[4] are the requested CIDs  
		if (getListedChunks(chunkSock, f, &reply[4], p_ad, p_hid) < 0) {
			status= -1;
			break;
		}
		if (gettimeofday(&tv, NULL) == 0)
			temp_end_msec = ((tv.tv_sec % 86400) * 1000 + tv.tv_usec / 1000);
					
		printf("Time elapses: %.3f seconds\n", (double)(temp_end_msec - temp_start_msec)/1000);
	}
	printf("Time elapses: %.3f seconds in total.\n", (double)(temp_end_msec - start_msec)/1000);	
	fclose(f);

	if (status < 0) {
		unlink(fin);
	}

	say("Received file %s\n", fout);
	sendCmd(sock, "done");
	Xclose(chunkSock);
	return status;
}

int initializeClient(const char *name) {
	int sock, rc;
	sockaddr_x dag;
	socklen_t daglen;
	char sdag[1024];
	char IP[MAX_XID_SIZE];

	// lookup the xia service 
	daglen = sizeof(dag);
	if (XgetDAGbyName(name, &dag, &daglen) < 0)
		die(-1, "unable to locate: %s\n", name);

	// create a socket, and listen for incoming connections
	if ((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0)
		die(-1, "Unable to create the listening socket\n");
    
	if (Xconnect(sock, (struct sockaddr*)&dag, daglen) < 0) {
		Xclose(sock);
		die(-1, "Unable to bind to the dag: %s\n", dag);
	}

	rc = XreadLocalHostAddr(sock, my_ad, MAX_XID_SIZE, my_hid, MAX_XID_SIZE, IP, MAX_XID_SIZE);

	if (rc < 0) {
		Xclose(sock);
		die(-1, "Unable to read local address.\n");
	} else{
		warn("My AD: %s, My HID: %s\n", my_ad, my_hid);
	}
	
	// save the AD and HID for later. This seems hacky
	// we need to find a better way to deal with this
	Graph g(&dag);
	strncpy(sdag, g.dag_string().c_str(), sizeof(sdag));
	// say("sdag = %s\n",sdag);
	char *ads = strstr(sdag,"AD:");
	char *hids = strstr(sdag,"HID:");
	// i = sscanf(ads,"%s",s_ad );
	// i = sscanf(hids,"%s", s_hid);
	
	if (sscanf(ads, "%s", s_ad ) < 1 || strncmp(s_ad, "AD:", 3) != 0) {
		die(-1, "Unable to extract AD.");
	}
		
	if (sscanf(hids,"%s", s_hid) < 1 || strncmp(s_hid, "HID:", 4) != 0) {
		die(-1, "Unable to extract AD.");
	}

	warn("Service AD: %s, Service HID: %s\n", s_ad, s_hid);
	return sock;
}

//FIXME Apparently XPutFile exists so use that instead.
//FIXME hardcoded ad-hid format for dag.
void putFile(int sock, char *ad, char *hid, const char *fin, const char *fout) {
	char cmd[512];
	sprintf(cmd, "put %s %s %s %s ",  ad,hid,fin,fout);
	sendCmd(sock, cmd);
	recvCmd((void *)&sock);
	say("done with put file\n");
}

void usage() {
	say("usage: get|put <source file> <dest name>\n");
}

bool file_exists(const char * filename) {
	if (FILE * file = fopen(filename, "r")) {
		fclose(file);
		return true;
	}
	return false;
}

int main(int argc, char **argv) {	
	const char *name;
	int sock = -1;
	char fin[512], fout[512];
	char cmd[512], reply[512];
	int params = -1;

	prefetch_ctx_sock = initializeClient(prefetch_ctx_name);

/*
	// say hello to the connext server
	char* hi = "Hello from context client";		
	sendCmd(prefetch_ctx_sock, hi);
	say("Say hi to prefetch client.\n");
*/
	say ("\n%s (%s): started\n", TITLE, VERSION);
	
	if (argc == 1) {
		say ("No service name passed, using default: %s\nYou can also pass --quick to execute a couple of default commands for quick testing. Requires s.txt to exist. \n", ftp_name);
		sock = initializeClient(ftp_name);
		usage();
	} else if (argc == 2) {
		if (strcmp(argv[1], "--quick") == 0) {
			quick = true;
			name = ftp_name;
		} else {
			name = argv[1];
			usage();
		}
		say ("Connecting to: %s\n", name);
		sock = initializeClient(name);
	} else if (argc == 3) {
			if (strcmp(argv[1], "--quick") == 0) {
				quick = true;
				name = argv[2];
				say ("Connecting to: %s\n", name);
				sock = initializeClient(name);
				usage();
			} else {
				die(-1, "xftp [--quick] [SID]");
			}
	} else {
		die(-1, "xftp [--quick] [SID]"); 
	}
	
	int i = 0;

	// This is for quick testing with a couple of commands
	while (i < NUM_PROMPTS) {
		say(">>");
		cmd[0] = '\n';
		fin[0] = '\n';
		fout[0] = '\n';
		params = -1;
		
		if (quick) {
			if (i == 0)
				strcpy(cmd, "put s.txt r.txt");
			else if (i == 1)
				strcpy(cmd, "get r.txt sr.txt\n");
			i++;
		} else {
			fgets(cmd, 511, stdin);
		}

		// compare the first three characters
		if (strncmp(cmd, "get", 3) == 0) {
			// params is the number of arguments
			params = sscanf(cmd, "get %s %s", fin, fout);
			if (params != 2) {
				sprintf(reply, "FAIL: invalid command (%s)\n", cmd);
				warn(reply);
				usage();
				continue;
			}
			if (strcmp(fin, fout) == 0) {
				warn("Since both applications write to the same folder (local case) the names should be different.\n");
				continue;
			}
			getFile(sock, s_ad, s_hid, fin, fout);
		}
		else if (strncmp(cmd, "put", 3) == 0) {
			params = sscanf(cmd,"put %s %s", fin, fout);
			if (params != 2) {
				sprintf(reply, "FAIL: invalid command (%s)\n", cmd);
				warn(reply);
				usage();
				continue;
			}
			if (strcmp(fin, fout) == 0) {
				warn("Since both applications write to the same folder (local case) the names should be different.\n");
				continue;
			}
			if (!file_exists(fin)) {
				warn("Source file: %s doesn't exist\n", fin);
				continue;
			}
			putFile(sock, my_ad, my_hid, fin, fout);
		}
		else {
			sprintf(reply, "FAIL: invalid command (%s)\n", cmd);
			warn(reply);
			usage();
		}
	}
	
	return 1;
}
