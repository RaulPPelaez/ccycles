.. _writing_a_bot:

Writing a bot
-------------
The interaction of a client bot and the server consists of the following steps:

1. Connection to the server
2. Receiving the game state from the server
3. Sending a player move to the server
4. Repeat steps 2. and 3. until the game is over
5. Disconnect

The server will kick clients (severing the connection) for two reasons:
1. Inactivity: If the client has not send a move in some time
2. Player termination: If the sent move was invalid (e.g. overlapping with other player)

The game time works in lock-step, the server will wait for reception of all players' moves before moving to the next time step. If a player takes too long to send its move, the player is terminated.


To write a bot, you have the following API functions available:

.. doxygenfile:: c_api.h

Example
*******

A simple bot that always moves north is shown below:

.. code-block:: c

		#include "c_api.h"
		#include <stdio.h>
		#include <stdlib.h>

		#define HOST "127.0.0.1"

		int main(int argc, char *argv[]) {
		  // Get the port from the environment variable CYCLES_PORT
		  const char *PORT = getenv("CYCLES_PORT");
		  const char *name = argc > 1 ? argv[1] : "NorthBot";
		  
		  if (PORT == NULL) {
		    fprintf(stderr, "Environment variable CYCLES_PORT not set.\n");
		    return EXIT_FAILURE;
		  }

		  cycles_onnection conn;
		  if (cycles_connect(name, HOST, PORT, &conn) < 0) {
		    fprintf(stderr, "Connection failed.\n");
		    return EXIT_FAILURE;
		  }

		  printf("Client connected as %s\n", conn.name);

		  // Game loop
		  for (;;) {
		    cycles_game_state gs;
		    if (cycles_recv_game_state(conn.sock, &gs) < 0) {
		      fprintf(stderr, "Failed to receive game state.\n");
		      break;
		    }
		    
		    printf("Frame %u: %u players\n", gs.frame_number, gs.player_count);
		    
		    // Always move north
		    if (cycles_send_move_i32(&conn, north) < 0) {
		      fprintf(stderr, "Failed to send move.\n");
		      free_game_state(&gs);
		      break;
		    }
		    
		    free_game_state(&gs);
		  }

		  cycles_disconnect(&conn);
		  return EXIT_SUCCESS;
		}

A more sophisticated example can be found in the `src/client/client_c_simple.c` file.


Other utilities
---------------


.. doxygenfile:: c_utils.h

		 
