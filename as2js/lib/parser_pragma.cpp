/* parser_pragma.cpp -- written by Alexis WILKE for Made to Order Software Corp. (c) 2005-2014 */

/*

Copyright (c) 2005-2014 Made to Order Software Corp.

http://snapwebsites.org/project/as2js

Permission is hereby granted, free of charge, to any
person obtaining a copy of this software and
associated documentation files (the "Software"), to
deal in the Software without restriction, including
without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the
following conditions:

The above copyright notice and this permission notice
shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF
ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO
EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include    "as2js/parser.h"
#include    "as2js/message.h"


namespace as2js
{


/**********************************************************************/
/**********************************************************************/
/***  PARSER PRAGMA  **************************************************/
/**********************************************************************/
/**********************************************************************/

void Parser::pragma()
{
    while(f_node->get_type() == Node::node_t::NODE_IDENTIFIER)
    {
        String const name = f_node->get_string();
        Node::pointer_t argument;
        bool prima = false;
        get_token();
        if(f_node->get_type() == Node::node_t::NODE_OPEN_PARENTHESIS)
        {
            // has zero or one argument
            get_token();
            // accept an empty argument '()'
            if(f_node->get_type() != Node::node_t::NODE_CLOSE_PARENTHESIS)
            {
                bool negative = false;
                if(f_node->get_type() == Node::node_t::NODE_SUBTRACT)
                {
                    get_token();
                    negative = true;
                }
                switch(f_node->get_type())
                {
                case Node::node_t::NODE_FALSE:
                case Node::node_t::NODE_STRING:
                case Node::node_t::NODE_TRUE:
                    if(negative)
                    {
                        negative = false;
                        Message msg(MESSAGE_LEVEL_ERROR, AS_ERR_BAD_PRAGMA, f_lexer->get_input()->get_position());
                        msg << "invalid negative argument for a pragma";
                    }
                    argument = f_node;
                    get_token();
                    break;

                case Node::node_t::NODE_FLOAT64:
                    argument = f_node;
                    if(negative)
                    {
                        argument->set_float64(-argument->get_float64().get());
                    }
                    get_token();
                    break;

                case Node::node_t::NODE_INT64:
                    argument = f_node;
                    if(negative)
                    {
                        argument->set_int64(-argument->get_int64().get());
                    }
                    get_token();
                    break;

                case Node::node_t::NODE_CLOSE_PARENTHESIS:
                {
                    Message msg(MESSAGE_LEVEL_ERROR, AS_ERR_BAD_PRAGMA, f_lexer->get_input()->get_position());
                    msg << "a pragma argument can't just be '-'";
                }
                    break;

                default:
                {
                    Message msg(MESSAGE_LEVEL_ERROR, AS_ERR_BAD_PRAGMA, f_lexer->get_input()->get_position());
                    msg << "invalid argument type for a pragma";
                }
                    break;

                }
            }
            if(f_node->get_type() == Node::node_t::NODE_CLOSE_PARENTHESIS)
            {
                Message msg(MESSAGE_LEVEL_ERROR, AS_ERR_BAD_PRAGMA, f_lexer->get_input()->get_position());
                msg << "invalid argument for a pragma";
            }
            else
            {
                get_token();
            }
        }
        if(f_node->get_type() == Node::node_t::NODE_CONDITIONAL)
        {
            prima = true;
            get_token();
        }

        // Check out this pragma. We have the following
        // info about each pragma:
        //
        //    name        The pragma name
        //    argument    The pragma argument (unknown by default)
        //    prima        True if pragma name followed by '?'
        //
        // NOTE: pragmas that we don't recognize are simply
        //       being ignored.
        //
        Options::option_value_t value(1);
        Options::option_t option = Options::OPTION_UNKNOWN;
        if(name == "extended_operators")
        {
            option = Options::OPTION_EXTENDED_OPERATORS;
        }
        else if(name == "no_extended_operators")
        {
            option = Options::OPTION_EXTENDED_OPERATORS;
            value = 0;
        }
        else if(name == "extended_escape_sequences")
        {
            option = Options::OPTION_EXTENDED_ESCAPE_SEQUENCES;
        }
        else if(name == "no_extended_escape_sequences")
        {
            option = Options::OPTION_EXTENDED_ESCAPE_SEQUENCES;
            value = 0;
        }
        else if(name == "octal")
        {
            option = Options::OPTION_OCTAL;
        }
        else if(name == "no_octal")
        {
            option = Options::OPTION_OCTAL;
            value = 0;
        }
        else if(name == "strict")
        {
            option = Options::OPTION_STRICT;
        }
        else if(name == "not_strict")
        {
            option = Options::OPTION_STRICT;
            value = 0;
        }
        else if(name == "trace_to_object")
        {
            option = Options::OPTION_TRACE_TO_OBJECT;
        }
        else if(name == "no_trace_to_object")
        {
            option = Options::OPTION_TRACE_TO_OBJECT;
            value = 0;
        }
        else if(name == "trace")
        {
            option = Options::OPTION_TRACE;
        }
        else if(name == "no_trace")
        {
            option = Options::OPTION_TRACE;
            value = 0;
        }
        if(option != Options::OPTION_UNKNOWN)
        {
            pragma_option(option, prima, argument, value);
        }
    }
}



void Parser::pragma_option(Options::option_t option, bool prima, Node::pointer_t& argument, Options::option_value_t value)
{
    // did we get any option object?
    if(f_options == 0)
    {
        // TBD: should we create one since we're dealing with options now?
        return;
    }

    // user overloaded the value?
    switch(argument->get_type())
    {
    case Node::node_t::NODE_UNKNOWN:
        // default value used
        break;

    case Node::node_t::NODE_TRUE:
        value = 1;
        break;

    case Node::node_t::NODE_INT64:
        value = option, argument->get_int64().get() != 0;
        break;

    case Node::node_t::NODE_FLOAT64:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
        value = option, argument->get_float64().get() != 0.0;
#pragma GCC diagnostic pop
        break;

    case Node::node_t::NODE_STRING:
    {
        Message msg(MESSAGE_LEVEL_ERROR, AS_ERR_INCOMPATIBLE_PRAGMA_ARGUMENT, f_lexer->get_input()->get_position());
        msg << "incompatible pragma argument";
    }
        break;

    default: // Node::node_t::NODE_FALSE
        value = 0;
        break;

    }

    if(prima)
    {
        if(f_options->get_option(option) != value)
        {
            Message msg(MESSAGE_LEVEL_ERROR, AS_ERR_PRAGMA_FAILED, f_lexer->get_input()->get_position());
            msg << "prima pragma failed";
        }
        return;
    }

    f_options->set_option(option, value);
}





}
// namespace as2js

// vim: ts=4 sw=4 et
