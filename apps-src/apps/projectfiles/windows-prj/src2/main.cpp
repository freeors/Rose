/* Require Rose v1.0.19 or above. $ */

#define GETTEXT_DOMAIN "studio-lib"

#include "base_instance.hpp"
#include "gui/dialogs/message.hpp"
#include "gui/dialogs/chat.hpp"
#include "gui/dialogs/home.hpp"
#include "gui/widgets/window.hpp"
#include "game_end_exceptions.hpp"
#include "wml_exception.hpp"
#include "gettext.hpp"
#include "loadscreen.hpp"
#include "formula_string_utils.hpp"
#include "help.hpp"


class game_instance: public base_instance
{
public:
	game_instance(rtc::PhysicalSocketServer& ss, int argc, char** argv);

};

game_instance::game_instance(rtc::PhysicalSocketServer& ss, int argc, char** argv)
	: base_instance(ss, argc, argv)
{
}


/**
 * Setups the game environment and enters
 * the titlescreen or game loops.
 */
static int do_gameloop(int argc, char** argv)
{
	rtc::PhysicalSocketServer ss;
	instance_manager<game_instance> manager(ss, argc, argv, "studio", "#rose");
	game_instance& game = manager.get();

	try {
		game.loadscreen_manager().reset();

		const font::floating_label_context label_manager;

		cursor::set(cursor::NORMAL);

		{
			gui2::thome dlg;
			dlg.show();
		}

		{
			// gui2::tchat2 chat("chat_module");
			// chat.show();
		}

	} catch (twml_exception& e) {
		e.show();

	} catch (CVideo::quit&) {
		//just means the game should quit
		SDL_Log("SDL_main, catched CVideo::quit\n");

	} catch (game_logic::formula_error& e) {
		gui2::show_error_message(e.what());
	} 

	return 0;
}

int main(int argc, char** argv)
{
	try {
		do_gameloop(argc, argv);
	} catch (twml_exception& e) {
		// this exception is generated when create instance.
		e.show();
	}

	return 0;
}