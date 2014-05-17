/*
* Copyright (C) 2014  HarpyWar (harpywar@gmail.com)
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
#include "common/setup_before.h"
#define GAME_INTERNAL_ACCESS

#include <cctype>
#include <cerrno>
#include <cstring>
#include <cstdlib>


#include "compat/strcasecmp.h"
#include "compat/snprintf.h"
#include "common/tag.h"
#include "common/util.h"
#include "common/eventlog.h"
#include "common/list.h"
#include "common/hashtable.h"

#include "connection.h"
#include "message.h"
#include "channel.h"
#include "game.h"
#include "account.h"
#include "account_wrap.h"
#include "timer.h"
#include "ipban.h"
#include "command_groups.h"
#include "friends.h"
#include "clan.h"
#include "attrlayer.h"
#include "icons.h"

#include "luawrapper.h"
#include "luaobjects.h"

#include "common/setup_after.h"


namespace pvpgn
{

	namespace bnetd
	{

		typedef enum {
			attr_type_str,
			attr_type_num,
			attr_type_bool,
			attr_type_raw
		} t_attr_type;


		/* Send message text to user */
		extern int __message_send_text(lua_State* L)
		{
			const char *text;
			const char *username_src, *username_dst;
			int message_type;
			t_connection *c_src = NULL, *c_dst = NULL;

			try
			{
				lua::stack st(L);
				// get args
				st.at(1, username_dst);
				st.at(2, message_type);
				st.at(3, username_src);
				st.at(4, text);
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}

			// get user connections
			if (t_account * account = accountlist_find_account(username_dst))
				c_dst = account_get_conn(account);

			if (username_src)
			if (t_account * account = accountlist_find_account(username_src))
				c_src = account_get_conn(account);

			// send message
			// (source can be NULL, but destination cant)
			if (c_dst)
				message_send_text(c_dst, (t_message_type)message_type, c_src, text);

			return 0;
		}

		/* Log text into logfile */
		extern int __eventlog(lua_State* L)
		{
			int loglevel;
			const char *text, *function;

			try
			{
				lua::stack st(L);
				// get args
				st.at(1, loglevel);
				st.at(2, function);
				st.at(3, text);
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}
			eventlog(t_eventlog_level(loglevel), function, text);

			return 0;
		}

		/* Get one account table object */
		extern int __account_get_by_name(lua_State* L)
		{
			const char *username;
			std::map<std::string, std::string> o_account;

			try
			{
				lua::stack st(L);
				// get args
				st.at(1, username);
				o_account = get_account_object(username);

				st.push(o_account);
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}

			return 1;
		}

		/* Get account attribute value */
		extern int __account_get_attr(lua_State* L)
		{
			const char *username, *attrkey;
			int attrtype;
			std::string attrvalue;
			std::map<std::string, std::string> o_account;

			try
			{
				lua::stack st(L);
				// get args
				st.at(1, username);
				st.at(2, attrkey);
				st.at(3, attrtype);

				if (t_account *account = accountlist_find_account(username))
				{
					switch ((t_attr_type)attrtype)
					{
					case attr_type_str:
						attrvalue = account_get_strattr(account, attrkey);
						break;
					case attr_type_num:
						attrvalue = std::to_string(account_get_numattr(account, attrkey));
						break;
					case attr_type_bool:
						attrvalue = account_get_boolattr(account, attrkey) == 0 ? "false" : "true";
						break;
					case attr_type_raw:
						attrvalue = account_get_rawattr(account, attrkey);
						break;
					}
				}

				st.push(attrvalue);
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}

			return 1;
		}

		/* Set account attribute value
		 * (Return 0 if attribute is not set, and 1 if set)
		 */
		extern int __account_set_attr(lua_State* L)
		{
			const char *username, *attrkey;
			int attrtype;
			std::map<std::string, std::string> o_account;
			int result = 0;

			try
			{
				lua::stack st(L);
				// get args
				st.at(1, username);
				st.at(2, attrkey);
				st.at(3, attrtype);

				if (t_account *account = accountlist_find_account(username))
				{
					switch ((t_attr_type)attrtype)
					{
					case attr_type_str:
						const char * strvalue;
						st.at(4, strvalue);

						if (account_set_strattr(account, attrkey, strvalue) >= 0)
							result = 1;
						break;
					case attr_type_num:
						int numvalue;
						st.at(4, numvalue);

						if (account_set_numattr(account, attrkey, numvalue) >= 0)
							result = 1;
						break;
					case attr_type_bool:
						bool boolvalue;
						st.at(4, boolvalue);

						if (account_set_boolattr(account, attrkey, boolvalue ? 1 : 0) >= 0)
							result = 1;
						break;
					case attr_type_raw:
						const char * rawvalue;
						int length;
						st.at(4, rawvalue);
						st.at(5, length);

						if (account_set_rawattr(account, attrkey, rawvalue, length) >= 0)
							result = 1;
						break;
					}
				}
				st.push(result);
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}
			return 1;
		}



		/* Get friend list of account */
		extern int __account_get_friends(lua_State* L)
		{
			const char *username;
			std::vector<std::map<std::string, std::string>> friends;

			try
			{
				lua::stack st(L);
				// get args
				st.at(1, username);

				if (t_account * account = accountlist_find_account(username))
				if (t_list *friendlist = account_get_friends(account))
				{
					t_elem const * curr;
					t_friend * f;
					LIST_TRAVERSE_CONST(friendlist, curr)
					{
						if (!(f = (t_friend*)elem_get_data(curr)))
						{
							eventlog(eventlog_level_error, __FUNCTION__, "found NULL entry in list");
							continue;
						}
						friends.push_back(get_friend_object(f));
					}
				}
				st.push(friends);
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}
			return 1;
		}


		/* Get team list of account */
		extern int __account_get_teams(lua_State* L)
		{
			const char *username;
			std::vector<std::map<std::string, std::string>> teams;

			try
			{
				lua::stack st(L);
				// get args
				st.at(1, username);

				if (t_account * account = accountlist_find_account(username))
				if (t_list *teamlist = account_get_teams(account))
				{
					t_elem const * curr;
					t_team * t;
					LIST_TRAVERSE_CONST(teamlist, curr)
					{
						if (!(t = (t_team*)elem_get_data(curr)))
						{
							eventlog(eventlog_level_error, __FUNCTION__, "found NULL entry in list");
							continue;
						}
						teams.push_back(get_team_object(t));
					}
				}
				st.push(teams);
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}
			return 1;
		}

		/* Get clan member list */
		extern int __clan_get_members(lua_State* L)
		{
			unsigned int clanid;
			std::vector<std::map<std::string, std::string>> members;

			try
			{
				lua::stack st(L);
				// get args
				st.at(1, clanid);

				if (t_clan * clan = clanlist_find_clan_by_clanid(clanid))
				if (t_list * clanmembers = clan_get_members(clan))
				{
					t_elem const * curr;
					LIST_TRAVERSE_CONST(clanmembers, curr)
					{
						t_clanmember *	m;
						if (!(m = (t_clanmember*)elem_get_data(curr)))
						{
							eventlog(eventlog_level_error, __FUNCTION__, "got NULL elem in list");
							continue;
						}
						members.push_back(get_clanmember_object(m));
					}
				}
				st.push(members);
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}
			return 1;
		}

		/* Get one game table object */
		extern int __game_get_by_id(lua_State* L)
		{
			unsigned int gameid;
			std::map<std::string, std::string> o_game;
			try
			{
				lua::stack st(L);
				// get args
				st.at(1, gameid);
				o_game = get_game_object(gameid);

				st.push(o_game);
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}

			return 1;
		}





		/* Get one channel table object */
		extern int __channel_get_by_id(lua_State* L)
		{
			unsigned int channelid;
			std::map<std::string, std::string> o_channel;
			try
			{
				lua::stack st(L);
				// get args
				st.at(1, channelid);
				o_channel = get_channel_object(channelid);

				st.push(o_channel);
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}

			return 1;
		}

		/* Get usernames online. If allaccounts = true then return all server users  */
		extern int __server_get_users(lua_State* L)
		{
			bool allaccounts = false;
			const char *username;
			std::map<unsigned int, std::string> users;
			t_connection * conn;
			t_account * account;

			try
			{
				lua::stack st(L);
				// get args
				st.at(1, allaccounts);

				if (allaccounts)
				{
					t_entry *   curr;
					HASHTABLE_TRAVERSE(accountlist(), curr)
					{
						if (account = (t_account*)entry_get_data(curr))
							users[account_get_uid(account)] = account_get_name(account);
					}
				}
				else
				{
					t_elem const * curr;
					LIST_TRAVERSE_CONST(connlist(), curr)
					{
						if (conn = (t_connection*)elem_get_data(curr))
							users[conn_get_userid(conn)] = conn_get_username(conn);
					}
				}
				st.push(users);
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}
			return 1;
		}
		/* Get game list table (id => name)  */
		extern int __server_get_games(lua_State* L)
		{
			std::map<unsigned int, std::string> games;
			t_game * game;

			try
			{
				lua::stack st(L);

				t_elist *   curr;
				elist_for_each(curr, gamelist())
				{
					if (game = elist_entry(curr, t_game, glist_link))
						games[game_get_id(game)] = game_get_name(game);
				}
				st.push(games);
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}
			return 1;
		}

		/* Get channel list (id => name)  */
		extern int __server_get_channels(lua_State* L)
		{
			std::map<unsigned int, std::string> channels;
			t_channel * channel;

			try
			{
				lua::stack st(L);

				t_elem *   curr;
				LIST_TRAVERSE(channellist(), curr)
				{
					if (channel = (t_channel*)elem_get_data(curr))
						channels[channel_get_channelid(channel)] = channel_get_name(channel);
				}
				st.push(channels);
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}
			return 1;
		}




		/* Get command groups for a command string */
		extern int __command_get_group(lua_State* L)
		{
			char const * command;
			try
			{
				lua::stack st(L);
				// get args
				st.at(1, command);
				int group = command_get_group(command);

				st.push(group);
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}
			return 1;
		}

		/* Get customicon rank by rating */
		extern int __icon_get_rank(lua_State* L)
		{
			int rating;
			char * clienttag;
			try
			{
				lua::stack st(L);
				// get args
				st.at(1, rating);
				st.at(2, clienttag);
				if (t_icon_info * icon = customicons_get_icon_by_rating(rating, clienttag))
					st.push(icon->rank);
			}
			catch (const std::exception& e)
			{
				eventlog(eventlog_level_error, __FUNCTION__, e.what());
			}
			catch (...)
			{
				eventlog(eventlog_level_error, __FUNCTION__, "lua exception\n");
			}
			return 1;
		}

	}
}