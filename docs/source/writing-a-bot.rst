.. _writing_a_bot:

Writing a bot
-------------

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

		  Connection conn;
		  if (cycles_connect(name, HOST, PORT, &conn) < 0) {
		    fprintf(stderr, "Connection failed.\n");
		    return EXIT_FAILURE;
		  }

		  printf("Client connected as %s\n", conn.name);

		  // Game loop
		  for (;;) {
		    GameState gs;
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

		 
