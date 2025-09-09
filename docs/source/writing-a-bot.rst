.. _writing_a_bot:

Writing a bot
-------------

To write a bot, you have the following API functions available:

.. doxygenfile:: c_api.h

Example
*******

A simple bot that always moves north is shown below:

.. code-block:: cpp

		#include "api.h"
		#include "utils.h"
		#include <string>
		#include <iostream>

		using namespace cycles;

		class BotClient {
		  Connection connection;
		  std::string name;
		  GameState state;

		  void sendMove() {
		    connection.sendMove(Direction::north);
		  }

		  void receiveGameState() {
		    state = connection.receiveGameState();
		    std::cout<<"There are "<<state.players.size()<<" players"<<std::endl;
		  }
		  
		public:
		  BotClient(const std::string &botName) : name(botName) {
		    connection.connect(name);
		    if (!connection.isActive()) {
		      exit(1);
		    }
		  }

		  void run() {
		    while (connection.isActive()) {
		      receiveGameState();
		      sendMove();
		    }
		  }

		};

		int main() {
		  BotClient bot("northton");
		  bot.run();
		return 0;
		}

A more sophisticated example can be found in the `src/client/client_c_simple.c` file.


Other utilities
---------------


.. doxygenfile:: c_utils.h

		 
