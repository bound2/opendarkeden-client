//----------------------------------------------------------------------
//
// Filename    : GameServerLink.h
// Description : Which packets a game server sends the client on the
//               game connection, the TCP link ClientPlayer reads in
//               CPS_NORMAL (docs/protocol/server-link.md). The rule is
//               the packet's link prefix: GC, game server to client.
//               Nothing this client only writes (CG), the login server's
//               LC replies, a peer's CR and RC packets or the updater's
//               UC and CU arrive there.
//
//               PacketValidator::init folds the rule over the factory
//               table, so a new GC packet is admitted once it is
//               registered and nothing else is; the server's
//               GameClientLink.h is the mirror for what a client sends.
//
//----------------------------------------------------------------------

#ifndef __GAME_SERVER_LINK_H__
#define __GAME_SERVER_LINK_H__

#include <string>

namespace GameServerLink {

// Whether a game server sends the packet named `packetName` on the game
// connection: the name's link prefix is GC.
inline bool SentByAGameServer ( const std::string & packetName )
{
	return packetName.size() >= 2 && packetName[0] == 'G' && packetName[1] == 'C';
}

} // namespace GameServerLink

#endif
