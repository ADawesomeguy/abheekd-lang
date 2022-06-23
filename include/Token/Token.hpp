//
// Created by abheekd on 6/21/2022.
//

#ifndef ABHEEK_LANG_TOKEN_HPP
#define ABHEEK_LANG_TOKEN_HPP

#include <string>

struct position {
    int row = 1; // line
    int column = 0;
};

class Token {
public:
    enum type {
        tok_eof = -1,

        // commands
        tok_func = -2,
        tok_extern = -3,
        tok_return = -4,

        // primary
        tok_ident = -20,
        tok_number = -21,
        tok_string = -22,

        //other
        tok_other = -100
    };

    Token();
    explicit Token(type type);
    Token(type type, std::string value, position position);

    type type;
    std::string value;
    position position;
};


#endif //ABHEEK_LANG_TOKEN_HPP