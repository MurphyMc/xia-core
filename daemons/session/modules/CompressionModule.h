/*
** Copyright 2013 Carnegie Mellon University
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

#ifndef COMPRESSION_MODULE_H
#define COMPRESSION_MODULE_H

#include "SessionModule.h"

class CompressionModule : public SessionModule {
	public:
		void decide(session::SessionInfo *sinfo, UserLayerInfo &userInfo, 
												 AppLayerInfo &appInfo, 
												 TransportLayerInfo &transportInfo, 
												 NetworkLayerInfo &netInfo, 
												 LinkLayerInfo &linkInfo, 
												 PhysicalLayerInfo &physInfo);

		bool breakpoint(Breakpoint breakpoint, struct breakpoint_context *context, void *rv);

	protected:
		bool preSend(struct breakpoint_context *context, void *rv);
		bool postRecv(struct breakpoint_context *context, void *rv);
		int compress(unsigned char *in, size_t in_len, unsigned char *out, size_t out_len);
		int decompress(unsigned char *in, size_t in_len, unsigned char *out, size_t out_len);
};

#endif /* COMPRESSION_MODULE_H */
