#pragma once
#include <cstdlib>
#include <iostream>

namespace LogUtilities
{
    bool supportsColors() {
        const char* term = std::getenv("TERM");
        return term && (std::string(term) == "xterm" || std::string(term) == "xterm-256color" || std::string(term) == "screen" || std::string(term) == "screen-256color");
    }
    struct COLOR_TEXT {
        static const std::string RESET;
        static const std::string BLACK;
        static const std::string RED;
        static const std::string GREEN;
        static const std::string YELLOW;
        static const std::string BLUE;
        static const std::string MAGENTA;
        static const std::string CYAN;
        static const std::string WHITE;
        static const std::string BRIGHT_BLACK;
        static const std::string BRIGHT_RED;
        static const std::string BRIGHT_GREEN;
        static const std::string BRIGHT_YELLOW;
        static const std::string BRIGHT_BLUE;
        static const std::string BRIGHT_MAGENTA;
        static const std::string BRIGHT_CYAN;
        static const std::string BRIGHT_WHITE;
    };

    // Define the color codes
    const std::string COLOR_TEXT::RESET = "\033[0m";
    const std::string COLOR_TEXT::BLACK = "\033[30m";
    const std::string COLOR_TEXT::RED = "\033[31m";
    const std::string COLOR_TEXT::GREEN = "\033[32m";
    const std::string COLOR_TEXT::YELLOW = "\033[33m";
    const std::string COLOR_TEXT::BLUE = "\033[34m";
    const std::string COLOR_TEXT::MAGENTA = "\033[35m";
    const std::string COLOR_TEXT::CYAN = "\033[36m";
    const std::string COLOR_TEXT::WHITE = "\033[37m";
    const std::string COLOR_TEXT::BRIGHT_BLACK = "\033[90m";
    const std::string COLOR_TEXT::BRIGHT_RED = "\033[91m";
    const std::string COLOR_TEXT::BRIGHT_GREEN = "\033[92m";
    const std::string COLOR_TEXT::BRIGHT_YELLOW = "\033[93m";
    const std::string COLOR_TEXT::BRIGHT_BLUE = "\033[94m";
    const std::string COLOR_TEXT::BRIGHT_MAGENTA = "\033[95m";
    const std::string COLOR_TEXT::BRIGHT_CYAN = "\033[96m";
    const std::string COLOR_TEXT::BRIGHT_WHITE = "\033[97m";



}
