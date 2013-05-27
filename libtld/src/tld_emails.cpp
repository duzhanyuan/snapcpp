/* TLD library -- TLD, emails extractions
 * Copyright (C) 2013  Made to Order Software Corp.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "libtld/tld.h"
#include <stdio.h>

/** \file
 * \brief Implementation of an email parser.
 *
 * This file includes all the functions available in the C library
 * of libtld. The format of emails is described in RFC 5322 paragraph
 * 3.4. That RFC uses the ABNF defined in RFC 5234. We limit our
 * implementation to reading a line of email addresses, not a full
 * email buffer. Thus we are limited to the content of a field such
 * as the "To:" field. We support emails that are written as:
 *
 * username@domain.tld
 * "First & Last Name" <username@domain.tld>
 *
 * And we support lists thereof (emails separated by commas.)
 *
 * Also, emails may include internationalized characters (Unicode). Since
 * our systems make use of UTF-8, the input format can be considered as
 * UTF-8 in which case we simply accept all characters from 0xA0 to
 * 0x10FFFF (the full Unicode range.) However, we also support the Q and B
 * encoding to directly support email fields. The B encoding is base64 of
 * UTF-8 data which works in ASCII 7 bit. The Q is ASCII with characters
 * marked with the equal sign and their 2 byte codes. This works well when
 * all the characters fit in one character set. Note that all characters
 * can be represented because more than one encoding can be used within
 * a phrase, but it is unlikely to be used that way.
 *
 * Text versions:
 *
 * http://www.ietf.org/rfc/rfc5322.txt
 * http://www.ietf.org/rfc/rfc5234.txt
 * http://www.ietf.org/rfc/rfc1522.txt
 *
 * HTML versions (with links):
 *
 * http://tools.ietf.org/html/rfc5322
 * http://tools.ietf.org/html/rfc5234
 * http://tools.ietf.org/html/rfc1522
 *
 * \note
 * At this point we do not foresee offering group capabilities. Therefore
 * the code does not support such. It will certainly be added later.
 * Note that the parser will skip all white spaces, including comments.
 * This means once parsed, all those white spaces and comments are lost.
 *
 * \note
 * The following code comes from a mix versions starting with RFC 2822
 * (http://www.ietf.org/rfc/rfc2822.txt) which still accepted all
 * control characters everywhere. Now only white spaces are allowed
 * in most places (\r\n\t and the space \x20). We also do not
 * allow control characters all over the place because it is likely
 * not valid.
 *
 * \code
 * (this part is not implemented, it just shows what is expected to be used for such
 * and such field.)
 * from            =       "From:" (mailbox-list / address-list) CRLF
 * sender          =       "Sender:" (mailbox / address) CRLF
 * reply-to        =       "Reply-To:" address-list CRLF
 * to              =       "To:" address-list CRLF
 * cc              =       "Cc:" address-list CRLF
 * bcc             =       "Bcc:" (address-list / [CFWS]) CRLF
 *
 * address         =       mailbox / group
 * mailbox         =       name-addr / addr-spec
 * name-addr       =       [display-name] angle-addr
 * angle-addr      =       [CFWS] "<" addr-spec ">" [CFWS] / obs-angle-addr
 * group           =       display-name ":" [mailbox-list / CFWS] ";" [CFWS]
 * display-name    =       phrase
 * mailbox-list    =       (mailbox *("," mailbox)) / obs-mbox-list
 * address-list    =       (address *("," address)) / obs-addr-list
 * addr-spec       =       local-part "@" domain
 * local-part      =       dot-atom / quoted-string / obs-local-part
 * domain          =       dot-atom / domain-literal / obs-domain
 * domain-literal  =       [CFWS] "[" *([FWS] dcontent) [FWS] "]" [CFWS]
 * dcontent        =       dtext / quoted-pair
 * dtext           =       NO-WS-CTL /     ; Non white space controls
 *                         %d33-90 /       ; The rest of the US-ASCII
 *                         %d94-126        ;  characters not including "[",
 *                                         ;  "]", or "\"
 * NO-WS-CTL       =       %d1-8 /         ; US-ASCII control characters
 *                         %d11 /          ;  that do not include the
 *                         %d12 /          ;  carriage return, line feed,
 *                         %d14-31 /       ;  and white space characters
 *                         %d127
 * text            =       %d1-9 /         ; Characters excluding CR and LF
 *                         %d11 /
 *                         %d12 /
 *                         %d14-127 /
 *                         obs-text
 * specials        =       "(" / ")" /     ; Special characters used in
 *                         "<" / ">" /     ;  other parts of the syntax
 *                         "[" / "]" /
 *                         ":" / ";" /
 *                         "@" / "\" /
 *                         "," / "." /
 *                         DQUOTE
 * DQUOTE          =       %x22
 * ALPHA           =       %x41-5A / %x61-7A   ; A-Z / a-z
 * DIGIT           =       %x30-39             ; 0-9
 * SP              =       %x20
 * HTAB            =       %x09
 * WSP             =       SP / HTAB
 * CR              =       %x0D
 * LF              =       %x0A
 * CRLF            =       CR LF
 * FWS             =       ([*WSP CRLF] 1*WSP) /   ; Folding white space
 *                         obs-FWS
 * quoted-pair     =       ("\" text) / obs-qp
 * ctext           =       NO-WS-CTL /     ; Non white space controls
 *                         %d33-39 /       ; The rest of the US-ASCII
 *                         %d42-91 /       ;  characters not including "(",
 *                         %d93-126        ;  ")", or "\"
 * ccontent        =       ctext / quoted-pair / comment / encoded-word
 * comment         =       "(" *([FWS] ccontent) [FWS] ")"
 * CFWS            =       *([FWS] comment) (([FWS] comment) / FWS)
 * atext           =       ALPHA / DIGIT / ; Any character except controls,
 *                         "!" / "#" /     ;  SP, and specials.
 *                         "$" / "%" /     ;  Used for atoms
 *                         "&" / "'" /
 *                         "*" / "+" /
 *                         "-" / "/" /
 *                         "=" / "?" /
 *                         "^" / "_" /
 *                         "`" / "{" /
 *                         "|" / "}" /
 *                         "~"
 * atom            =       [CFWS] 1*atext [CFWS]
 * dot-atom        =       [CFWS] dot-atom-text [CFWS]
 * dot-atom-text   =       1*atext *("." 1*atext)
 * qtext           =       NO-WS-CTL /     ; Non white space controls
 *                         %d33 /          ; The rest of the US-ASCII
 *                         %d35-91 /       ;  characters not including "\"
 *                         %d93-126        ;  or the quote character
 * qcontent        =       qtext / quoted-pair
 * quoted-string   =       [CFWS]
 *                         DQUOTE *([FWS] qcontent) [FWS] DQUOTE
 *                         [CFWS]
 * word            =       atom / quoted-string
 * phrase          =       1*word / obs-phrase
 *
 * # Added by RFC-1522
 * encoded-word    =       "=?" charset "?" encoding "?" encoded-text "?="
 * charset         =       token
 * encoding        =       token
 * token           =       1*<Any CHAR except SPACE, CTLs, and especials>
 *                         ; equivalent to:
 *                         ; 1*(%d33 / %d35-39 / %d42-43 / %d45 / %d48-57 /
 *                         ; %d65-90 / %d92 / %d94-126)
 * especials       =       "(" / ")" / "<" / ">" / "@" / "," / ";" / ":" /
 *                         DQUOTE / "/" / "[" / "]" / "?" / "." / "="
 * encoded-text    =       1*<Any printable ASCII character other than "?" or SPACE>
 *                         ; %d33-62 / %d64-126
 *
 * # Obsolete syntax "extensions"
 * obs-from        =       "From" *WSP ":" mailbox-list CRLF
 * obs-sender      =       "Sender" *WSP ":" mailbox CRLF
 * obs-reply-to    =       "Reply-To" *WSP ":" mailbox-list CRLF
 * obs-to          =       "To" *WSP ":" address-list CRLF
 * obs-cc          =       "Cc" *WSP ":" address-list CRLF
 * obs-bcc         =       "Bcc" *WSP ":" (address-list / [CFWS]) CRLF
 * obs-qp          =       "\" (%d0-127)
 * obs-text        =       *LF *CR *(obs-char *LF *CR)
 * obs-char        =       %d0-9 / %d11 /          ; %d0-127 except CR and
 *                         %d12 / %d14-127         ;  LF
 * obs-utext       =       obs-text
 * obs-phrase      =       word *(word / "." / CFWS)
 * obs-phrase-list =       phrase / 1*([phrase] [CFWS] "," [CFWS]) [phrase]
 * obs-FWS         =       1*WSP *(CRLF 1*WSP)
 * obs-angle-addr  =       [CFWS] "<" [obs-route] addr-spec ">" [CFWS]
 * obs-route       =       [CFWS] obs-domain-list ":" [CFWS]
 * obs-domain-list =       "@" domain *(*(CFWS / "," ) [CFWS] "@" domain)
 * obs-local-part  =       word *("." word)
 * obs-domain      =       atom *("." atom)
 * obs-mbox-list   =       1*([mailbox] [CFWS] "," [CFWS]) [mailbox]
 * obs-addr-list   =       1*([address] [CFWS] "," [CFWS]) [address]
 * \endcode
 *
 * The ABNF is a bit complicated to use as is, so there is a lex and yacc
 * which I find easier to implement to my point of view:
 *
 * \code
 * (lex part)
 * [-A-Za-z0-9!#$%&'*+/=?^_`{|}~]+                                          atom_text_repeat (ALPHA+DIGIT+some other characters)
 * ([\x09\x0A\x0D\x20-\x27\x2A-\x5B\x5D-\x7E]|\\[\x09\x20-\x7E])+           comment_text_repeat
 * ([\x33-\x5A\x5E-\x7E])+                                                  domain_text_repeat
 * ([\x21\x23-\x5B\x5D-\x7E]|\\[\x09\x20-\x7E])+                            quoted_text_repeat
 * \x22                                                                     DQUOTE
 * [\x20\x09]*\x0D\x0A[\x20\x09]+                                           FWS
 * .                                                                        any other character
 *
 * (lex definitions merged in more complex lex definitions)
 * [\x01-\x08\x0B\x0C\x0E-\x1F\x7F]                                         NO_WS_CTL
 * [()<>[\]:;@\\,.]                                                         specials
 * [\x01-\x09\x0B\x0C\x0E-\x7F]                                             text
 * \\[\x09\x20-\x7E]                                                        quoted_pair ('\\' text)
 * [A-Za-z]                                                                 ALPHA
 * [0-9]                                                                    DIGIT
 * [\x20\x09]                                                               WSP
 * \x20                                                                     SP
 * \x09                                                                     HTAB
 * \x0D\x0A                                                                 CRLF
 * \x0D                                                                     CR
 * \x0A                                                                     LF
 *
 * (yacc part)
 * address_list: address
 *             | address ',' address_list
 * address: mailbox
 *        | group
 * mailbox_list: mailbox
 *             | mailbox ',' mailbox_list
 * mailbox: name_addr
 *        | addr_spec
 * group: display_name ':' mailbox_list ';' CFWS
 *      | display_name ':' CFWS ';' CFWS
 * name_addr: angle_addr
 *          | display_name angle_addr
 * display_name: phrase
 * angle_addr: CFWS '<' addr_spec '>' CFWS
 * addr_spec: local_part '@' domain
 * local_part: dot_atom
 *           | quoted_string
 * domain: dot_atom
 *       | domain_literal
 * domain_literal: CFWS '[' FWS domain_text_repeat FWS ']' CFWS
 * phrase: word
 *       | word phrase
 * word: atom
 *     | quoted_string
 * atom: CFWS atom_text_repeat CFWS
 * dot_atom: CFWS dot_atom_text CFWS
 * dot_atom_text: atom_text_repeat
 *              | atom_text_repeat '.' dot_atom_text
 * quoted_string: CFWS DQUOTE quoted_text_repeat DQUOTE CFWS
 * CFWS: <empty>
 *     | FWS comment
 *     | CFWS comment FWS
 * comment: '(' comment_content ')'
 * comment_content: comment_text_repeat
 *                | comment
 *                | ccontent ccontent
 * \endcode
 */





namespace
{
/** \brief Internal function used to trim a string.
 *
 * This function is used to remove any white spaces (\r, \n, \t, and
 * spaces (\x20)) from the end of the string passed in as a parameter.
 *
 * The function makes use of the resize() function if any character
 * need to be removed.
 *
 * \param[in,out] value  The string to be trimmed
 */
void trim(std::string& value)
{
    if(!value.empty())
    {
        size_t i(value.length() - 1);
        for(; i > 0; --i)
        {
            const char c(value[i]);
            if(c != ' ' && c != '\r' && c != '\n' && c != '\t')
            {
                ++i;
                break;
            }
        }
        value.resize(i);
    }
}

/** \brief Check whether a character can be quoted.
 *
 * The quoted characters are visible characters and white spaces (space 0x20,
 * and horizontal tab 0x09).
 *
 * \param[in] c  The character being escaped to know whether it can be.
 *
 * \return true if the character can be used with \, false otherwise
 */
bool is_quoted_char(char c)
{
    // 0x7F is the Delete key which is viewed as a control
    // here we accept all characters over 127 in case the user
    // has UTF-8 as input data
    return c == '\t' || c >= ' ' && c != 0x7F;
}
}


/** \brief Initialize the tld_email_list object.
 *
 * This function initializes the tld_email_list object appropriately.
 *
 * By default a tld_email_list object is empty so the next() function
 * returns false immediately and the count() function returns zero (0).
 */
tld_email_list::tld_email_list()
    //: f_input("") -- auto-init
    : f_flags(0)
    , f_result(TLD_RESULT_SUCCESS)
    //, f_last_group("") -- auto-init
    , f_pos(0)
    //, f_email_list() -- auto-init
{
}

/** \brief Parse a new list of emails.
 *
 * This function parses the list of emails as specified by \p emails.
 * The result is TLD_RESULT_SUCCESS if all the email addresses were
 * valid. Any other result means that the resulting list of email
 * addresses will be completely empty.
 *
 * Note that at this time it is not possible to only extra the list
 * of valid emails from a list of valid and invalid emails.
 *
 * \param[in] emails  A list of email address to be parsed.
 * \param[in] flags  A set of flags to define what should be checked and what should be ignored.
 */
tld_result tld_email_list::parse(const std::string& emails, int flags)
{
    f_input = emails;
    f_flags = flags;
    f_result = TLD_RESULT_SUCCESS;
    f_last_group.clear();
    f_pos = 0; // always rewind too
    f_email_list.clear();

    parse_all_emails();
    if(f_result != TLD_RESULT_SUCCESS)
    {
        f_email_list.clear();
    }

    return f_result;
}

/** \brief Parse all the emails in f_input.
 *
 * This function reads all the emails found in the f_input string. It
 * generates a list of emails segregated by group.
 */
void tld_email_list::parse_all_emails()
{
    // old emails supposedly accepted \0 in headers! we do not
    // we actually don't even support control characters as
    // defined in the newest version of the Internet Message
    // (RFC 5322); the following loop, though, does not check
    // all the characters, only those necessary to cut all the
    // email elements properly

    const char *start(f_input.c_str());
    bool group(true);
    const char *s(start);
    for(; *s != '\0'; ++s)
    {
        switch(*s)
        {
        case ' ':
        case '\n':
        case '\r':
        case '\t':
            // skip leading spaces immediately
            if(start == s)
            {
                start = s + 1;
            }
            break;

        case ';':
            // end of this group
            {
                // trim ending spaces
                const char *end(s);
                for(; end > start; --end)
                {
                    const char c(end[-1]);
                    if(c != ' ' && c != '\n' && c != '\r' && c != '\t')
                    {
                        break;
                    }
                }
                if(end - start > 0)
                {
                    std::string e(start, end - start);
                    tld_email_t email;
                    email.f_group = f_last_group;
                    f_result = email.parse(e);
                    if(f_result != TLD_RESULT_SUCCESS)
                    {
                        return;
                    }
                    f_email_list.push_back(email);
                }
            }
            f_last_group = "";
            group = true;
            start = s + 1;
            break;

        case ':':
            // group label
            if(!group)
            {
                // wrong place for this ':' character
                f_result = TLD_RESULT_INVALID;
                return;
            }
            {
                // trim ending spaces
                const char *end(s);
                for(; end > start; --end)
                {
                    const char c(end[-1]);
                    if(c != ' ' && c != '\n' && c != '\r' && c != '\t')
                    {
                        break;
                    }
                }
                if(end - start <= 0)
                {
                    // An explicitly empty group name is not legal
                    f_result = TLD_RESULT_INVALID;
                    return;
                }
                std::string last_group(start, end - start);
                // always add the group with an empty email (in case there
                // is no email; and it clearly delimit each group.)
                tld_email_t email;
                f_result = email.parse_group(last_group);
                if(f_result != TLD_RESULT_SUCCESS)
                {
                    // this happens if the group name is invalid
                    // (i.e. include controls or is empty)
                    return;
                }
                f_last_group = email.f_group;
                f_email_list.push_back(email);
            }
            start = s + 1;
            group = false; // cannot get another legal ':' until we find the ';'
            break;

        case ',':
            // email separation
            {
                // trim ending spaces
                const char *end(s);
                for(; end > start; --end)
                {
                    const char c(end[-1]);
                    if(c != ' ' && c != '\n' && c != '\r' && c != '\t')
                    {
                        break;
                    }
                }
                if(end - start > 0)
                {
                    std::string e(start, end - start);
                    tld_email_t email;
                    email.f_group = f_last_group;
                    f_result = email.parse(e);
                    if(f_result != TLD_RESULT_SUCCESS)
                    {
                        return;
                    }
                    f_email_list.push_back(email);
                }
            }
            start = s + 1;
            break;

        case '"':
            // quoted strings may include escaped characters so it is a
            // special case, also it could include a comma
            for(++s; *s != '\0' && *s != '"'; ++s)
            {
                if(*s == '\\')
                {
                    if(!is_quoted_char(s[1]))
                    {
                        // "\NUL" is never considered valid
                        f_result = TLD_RESULT_INVALID;
                        return;
                    }
                    ++s;
                }
            }
            if(*s == '\0')
            {
                // unterminated quoted string
                f_result = TLD_RESULT_INVALID;
                return;
            }
            break;

        case '(':
            {
                // comments may include other comments
                int comment_count(1);
                for(++s; *s != '\0' && comment_count > 0; ++s)
                {
                    if(*s == '\\')
                    {
                        if(!is_quoted_char(s[1]))
                        {
                            // "\NUL" is never considered valid
                            f_result = TLD_RESULT_INVALID;
                            return;
                        }
                        ++s;
                    }
                    else if(*s == '(')
                    {
                        ++comment_count;
                    }
                    else if(*s == ')')
                    {
                        --comment_count;
                    }
                }
                if(*s == '\0')
                {
                    // unterminated comment
                    f_result = TLD_RESULT_INVALID;
                    return;
                }
            }
            break;

        case '[':
            for(++s; *s != ']'; ++s)
            {
                if(*s == '\0' || *s == '[' || *s == '\\')
                {
                    // domain literal cannot include '[', ']', or '\'
                    // and it must end with ']'
                    f_result = TLD_RESULT_INVALID;
                    return;
                }
            }
            break;

        }
    }

    if(!group)
    {
        // the ';' to end a group is missing
        f_result = TLD_RESULT_INVALID;
        return;
    }

    {
        // trim ending spaces
        const char *end(s);
        for(; end > start; --end)
        {
            const char c(end[-1]);
            if(c != ' ' && c != '\n' && c != '\r' && c != '\t')
            {
                break;
            }
        }
        if(end - start > 0)
        {
            std::string e(start, end - start);
            tld_email_t email;
            email.f_group = f_last_group;
            f_result = email.parse(e);
            if(f_result != TLD_RESULT_SUCCESS)
            {
                return;
            }
            f_email_list.push_back(email);
        }
    }
}

/** \brief Return the number of emails recorded.
 *
 * This function returns the number of times the next() function can be
 * called to retrieve all the groups and emails. Note that this count
 * include group entries (i.e. entries with a group name but no email
 * addresses.)
 *
 * \return The number of items in the list of emails, including groups.
 *
 * \sa next()
 */
int tld_email_list::count() const
{
    return static_cast<int>(f_email_list.size());
}

/** \brief Rewind the reader to the start of the list.
 *
 * This function reset the reader position back to the beginning of
 * the list of emails. The position increases each time the next()
 * function returns true.
 *
 * \sa next()
 */
void tld_email_list::rewind() const
{
    f_pos = 0;
}

/** \brief Retrieve a copy of the next email information.
 *
 * This function reads the next email in your \p e parameter.
 *
 * The function returns true when the email parameter could be set. It
 * is very important that you check that return value because otherwise
 * you cannot actually know whether you reached the end of the list.
 *
 * \param[out] e  The email object that receives the next item if there is one.
 *
 * \return true if e was set, false otherwise and e is not modified.
 */
bool tld_email_list::next(tld_email_t& e) const
{
    if(f_pos >= static_cast<int>(f_email_list.size()))
    {
        return false;
    }

    e = f_email_list[f_pos];
    ++f_pos;

    return true;
}

/** \brief Retrieve a copy of the next email information.
 *
 * This function reads the next email in your \p e parameter.
 *
 * The function returns true when the email parameter could be set. It
 * is very important that you check that return value because otherwise
 * you cannot actually know whether you reached the end of the list.
 *
 * \warning
 * The pointers saved in the tld_email structure are taken from the
 * list of emails defined in the tld_email_list object. If the list
 * is changed (by a call to the parse() function) then those pointers
 * become invalid.
 *
 * \param[out] e  The email object that receives the next item if there is one.
 *
 * \return true if e was set, false otherwise and e is not modified.
 */
bool tld_email_list::next(tld_email *e) const
{
    if(f_pos >= static_cast<int>(f_email_list.size()))
    {
        return false;
    }

    e->f_group             = f_email_list[f_pos].f_group.c_str();
    e->f_original_email    = f_email_list[f_pos].f_original_email.c_str();
    e->f_fullname          = f_email_list[f_pos].f_fullname.c_str();
    e->f_username          = f_email_list[f_pos].f_username.c_str();
    e->f_domain            = f_email_list[f_pos].f_domain.c_str();
    e->f_email_only        = f_email_list[f_pos].f_email_only.c_str();
    e->f_canonalized_email = f_email_list[f_pos].f_canonalized_email.c_str();
    ++f_pos;

    return true;
}


/** \brief Parse one email to a tld_email_t object.
 *
 * The \p email parameter is expected to represent exactly one email.
 * This function is expected to only be used by the tld_email_list
 * parser with valid data, although it is definitively not forbidden
 * to make use of this function, you may find it more difficult to
 * use directly.
 *
 * \note
 * If the email is not valid, then the tld_email_t object remains
 * unchanged.
 *
 * \exception std::logic_error
 * If a quoted string or a comment have an unexpected character in
 * them then this exception is raised. If you are calling this
 * function directly then you may get this exception. If you called
 * the parse() function of the tld_email_list then this exception
 * should never happen because the previous level captures those
 * errors already (hence the exception.)
 *
 * \param[in] email  The email to be parsed.
 *
 * \return The result of the parsing, TLD_RESULT_SUCCESS on success,
 * another value otherwise.
 */
tld_result tld_email_list::tld_email_t::parse(const std::string& email)
{
    // The following is parsing ONE email since we already removed the
    // groups, commas, semi-colons, leading and ending spaces.
    std::string value;
    value.reserve(email.length());
    std::string fullname;
    std::string username;
    std::string domain;
    int count;
    bool has_angle(false);
    bool found_at(false);
    bool found_dot(false);
    bool done(false);
    const char *start(email.c_str());
    const char *s(start);
    for(; *s != '\0'; ++s)
    {
        switch(*s)
        {
        case '"':
            if(done)
            {
                return TLD_RESULT_INVALID;
            }
            for(++s; *s != '"'; ++s)
            {
                if(*s == '\0')
                {
                    throw std::logic_error("somehow we found a \\0 in a quoted string in tld_email_t which should not happen since it was already checked in tld_email_list::parse()");
                }
                if(*s == '\\')
                {
                    // the backslash is not part of the result
                    ++s;
                }
                if((static_cast<unsigned char>(*s) < ' ' && *s != '\t') || *s == 0x7F)
                {
                    // do not accept any control characters
                    // (note that this is sufficient to check all characters
                    // after the \ character)
                    return TLD_RESULT_INVALID;
                }
                value += *s;
            }
            break;

        case '(':
            // comments are completely ignored
            count = 1;
            for(++s; count > 0; ++s)
            {
                char c(*s);
                switch(c)
                {
                case '\0':
                    throw std::logic_error("somehow we found a \\0 in a comment in tld_email_t which should not happen since it was already checked in tld_email_list::parse()");

                case '(':
                    ++count;
                    break;

                case ')':
                    --count;
                    break;

                case '\n':
                case '\r':
                case '\t':
                    c = ' ';
                    break;

                case '\\':
                    ++s;
                    if(!is_quoted_char(*s))
                    {
                        throw std::logic_error("somehow we found a \\0 in a comment quoted pair in tld_email_t which should not happen since it was already checked in tld_email_list::parse()");
                    }
                    c = *s;
                    break;

                }
                if(static_cast<unsigned char>(c) < ' ')
                {
                    // do not accept any control characters in comments
                    // (except \r, \n, and \t)
                    return TLD_RESULT_INVALID;
                }
            }
            --s;
            break;

        case '[':
            if(!found_at || done || !value.empty() || !domain.empty())
            {
                // domain before the '@'
                return TLD_RESULT_INVALID;
            }
            for(++s; *s != ']'; ++s)
            {
                const char c(*s);
                if(c != ' ' && c != '\n' && c != '\r' && c != '\t')
                {
                    break;
                }
            }
            for(; *s != '[' && *s != '\\' && *s != ']' && *s != ' ' && *s != '\n' && *s != '\r' && *s != '\t'; ++s)
            {
                if(*s == '\0')
                {
                    throw std::logic_error("somehow we found a \\0 in a literal domain in tld_email_t which should not happen since it was already checked in tld_email_list::parse()");
                }
                if(static_cast<unsigned char>(*s) < ' ' || *s == 0x7F)
                {
                    // do not accept any control characters
                    return TLD_RESULT_INVALID;
                }
                value += *s;
            }
            // we can have spaces at the end, but those must be followed by ']'
            for(; *s != '[' && *s != '\\' && *s != ']'; ++s)
            {
                const char c(*s);
                if(c != ' ' && c != '\n' && c != '\r' && c != '\t')
                {
                    break;
                }
            }
            if(*s != ']' || value.empty())
            {
                // domain literal cannot include a space and other characters
                // nor can it be empty
                return TLD_RESULT_NULL;
            }
            domain = value;
            value.clear();
            break;

        case '<':
            if(has_angle || found_at || found_dot || done)
            {
                // found two '<' or the '<' after the '@'
                // or we had a dot before meaning that we already have a dotted username
                return TLD_RESULT_INVALID;
            }

            // if we have an angle email address, whatever we found so far
            // is the user name; although it can be empty
            trim(value);
            if(!value.empty())
            {
                fullname = value;
                value.clear();
            }
            has_angle = true;
            break;

        case '>':
            if(!has_angle || !found_at || done)
            {
                // missing '<' and/or '@'
                return TLD_RESULT_INVALID;
            }
            if(domain.empty())
            {
                trim(value);
                if(value.empty())
                {
                    // an empty domain name is not valid, apparently
                    return TLD_RESULT_NULL;
                }
                // we are done, we can only find spaces and comments
                domain = value;
            }
            else
            {
                if(!value.empty())
                {
                    return TLD_RESULT_INVALID;
                }
            }
            done = true;
            has_angle = false;
            value.clear();
            break;

        case '@':
            // Note: if done is true, found_at is also true here
            if(found_at || done)
            {
                // found two '@' characters
                return TLD_RESULT_INVALID;
            }
            found_at = true;
            found_dot = false; // reset this flag
            trim(value);
            if(value.empty())
            {
                // no username is not a valid entry
                return TLD_RESULT_NULL;
            }
            username = value;
            value.clear();
            break;

        case ' ':
        case '\n':
        case '\r':
        case '\t':
            // keep just one space
            if(!value.empty())
            {
                value += ' ';
            }
            // and skip all the others
            // (as far as I know this is not allowed in the RFC, only one space
            // between items; however, after a new-line / carriage return, you
            // could get many spaces and tabs and that's legal)
            for(++s; *s != '\0'; ++s)
            {
                const char c(*s);
                if(c != ' ' && c != '\n' && c != '\r' && c != '\t')
                {
                    break;
                }
            }
            --s;
            break;

        case '.':
            if(value.empty()                                // cannot start with a dot
            || (!value.empty() && *value.rbegin() == '.')   // cannot include two dots one after the other
            || s[1] == '@' || s[1] == '>')                  // cannot end with a dot
            {
                return TLD_RESULT_INVALID;
            }
            found_dot = true;
            value += '.';
            break;

        default:
            // here we must have a valid atom character ([-A-Za-z0-9!#$%&'*+/=?^_`{|}~])
            if((*s < 'A' || *s > 'Z')
            && (*s < 'a' || *s > 'z')
            && (*s < '0' || *s > '9')
            && *s != '!' && *s != '#'
            && *s != '$' && *s != '%'
            && *s != '&' && *s != '\''
            && *s != '*' && *s != '+'
            && *s != '-' && *s != '/'
            && *s != '=' && *s != '?'
            && *s != '^' && *s != '_'
            && *s != '`' && *s != '{'
            && *s != '|' && *s != '}'
            && *s != '~')
            {
                // not a valid atom character
                return TLD_RESULT_INVALID;
            }
            value += *s;
            break;

        }
    }

    if(username.empty() || has_angle)
    {
        // no username means the '@' is missing
        // angle bracket was not closed
        return TLD_RESULT_NULL;
    }

    if(done)
    {
        if(!value.empty())
        {
            // nothing can appear after the domain
            return TLD_RESULT_INVALID;
        }
    }
    else
    {
        trim(value);
        if(value.empty())
        {
            if(domain.empty())
            {
                // domain is missing
                return TLD_RESULT_NULL;
            }
        }
        else
        {
            if(!domain.empty())
            {
                // domain "defined twice"
                return TLD_RESULT_INVALID;
            }
            domain = value;
        }
    }

    // finally, verify that the domain is indeed valid
    // (i.e. proper characters, structure, and TLD)
    struct tld_info info;
    tld_result result(tld(domain.c_str(), &info));
    if(result != TLD_RESULT_SUCCESS)
    {
        return result;
    }

    f_original_email = email;
    f_fullname = fullname;
    f_username = username;
    f_domain = domain;
    f_email_only = username + "@" + domain;  // TODO protect characters...
    if(fullname.empty())
    {
        f_canonalized_email = f_email_only;
    }
    else
    {
        f_canonalized_email = "\"" + fullname + "\" <" + f_email_only + ">";  // TODO protect characters...
    }

    return TLD_RESULT_SUCCESS;
}

/** \brief Parse a group including comments.
 *
 * This function parses a group name and remove comments and
 * double spaces, and replace all white spaces with character 0x20.
 *
 * The function also verifies that the input string does not include
 * characters that are considered illegal in a group name such as
 * controls.
 *
 * Note that the name of the group cannot be empty because when this
 * function is called, it is expected to preceed the colon (:) character.
 *
 * \exception std::logic_error
 * This exception is raised if the function detects an invalid comment.
 * This function is not expected to be called directly so comments should
 * never be wrong since these are checked in the parse_all_emails()
 * function and thus cannot logically be wrong here.
 *
 * \param[in] group  The name of the group to be parsed.
 *
 * \return Whether the function succeeded (TLD_RESULT_SUCCESS) or
 * failed (TLD_RESULT_INVALID).
 */
tld_result tld_email_list::tld_email_t::parse_group(const std::string& group)
{
    const char *s(group.c_str());
    std::string g;
    int count;

    for(; *s != '\0'; ++s)
    {
        switch(*s)
        {
        case ' ':
        case '\n':
        case '\r':
        case '\t':
            if(!g.empty())
            {
                g += ' ';
            }
            for(++s; *s == ' ' || *s == '\n' || *s == '\r' || *s == '\t'; ++s);
            --s;
            break;

        case '(':
            count = 1;
            for(++s; count > 0; ++s)
            {
                if(*s == '\0')
                {
                    throw std::logic_error("somehow we found a \\0 in a quoted string in tld_email_t which should not happen since it was already checked in tld_email_list::parse()");
                }
                switch(*s)
                {
                case '(':
                    ++count;
                    break;

                case ')':
                    --count;
                    break;

                case '\\':
                    if(!is_quoted_char(s[1]))
                    {
                        throw std::logic_error("somehow we found a \\0 in a comment in tld_email_t which should not happen since it was already checked in tld_email_list::parse()");
                    }
                    ++s;
                    break;

                // controls, etc. were already checked
                }
            }
            // come back on the ')' since the main for will do a ++s
            --s;
            break;

        default:
            if(static_cast<unsigned char>(*s) < ' ' || *s == 0x7F)
            {
                return TLD_RESULT_INVALID;
            }
            g += *s;
            break;

        }
    }
    if(g.empty())
    {
        return TLD_RESULT_INVALID;
    }

    f_group = g;

    return TLD_RESULT_SUCCESS;
}

/** \brief Allocate a list of emails object.
 *
 * This function allocates a list of emails object that can then be
 * used to parse a string representing a list of emails and retrieve
 * those emails with the use of the tld_email_next() function.
 *
 * \note
 * The object is a C++ class.
 *
 * \return A pointer to a list of emails object.
 *
 * \sa tld_email_next()
 */
struct tld_email_list *tld_email_alloc()
{
    return new tld_email_list;
}

/** \brief Free the list of emails.
 *
 * This function frees the list of emails as allocated by the
 * tld_email_alloc(). Afterward the \p list pointer is not valid
 * anymore.
 *
 * \param[in] list  The list to be freed.
 */
void tld_email_free(struct tld_email_list *list)
{
    delete list;
}

/** \brief Parse a list of emails in the email list object.
 *
 * This function parses the email listed in the \p emails parameter
 * and saves the result in the list parameter. The function saves
 * the information as a list of email list in the \p list object.
 *
 * \param[in] list  The list of emails object.
 * \param[in] emails  The list of emails to be parsed.
 * \param[in] flags  The flags are used to change the behavior of the parser.
 */
tld_result tld_email_parse(struct tld_email_list *list, const char *emails, int flags)
{
    return list->parse(emails, flags);
}

/** \brief Return the number of emails found after a parse.
 *
 * This function returns the number of emails that were found in the list
 * of emails passed to the tld_email_parse() function.
 *
 * \param[in] list  The email list object.
 *
 * \return The number of emails defined in the object, it may be zero.
 */
int tld_email_count(struct tld_email_list *list)
{
    return list->count();
}

/** \brief Rewind the reading of the emails.
 *
 * This function resets the position to the start of the list.
 * The next call to the tld_email_next() function will return
 * the first email again.
 *
 * \param[in] list  The list of email object to reset.
 */
void tld_email_rewind(struct tld_email_list *list)
{
    list->rewind();
}

/** \brief Retrieve the next email.
 *
 * This function retrieves the next email found when parsing the emails
 * passed to to the tld_email_parse() function. The function returns
 * 1 when another email was defined. It returns 0 when no more emails
 * exist and the \p e parameter does not get set. The function can be
 * called any number of times after it returned zero (0).
 *
 * \param[in] list  The list from which the email is to be read.
 * \param[out] email  The buffer where the email is to be written.
 *
 * \sa tld_email_parse()
 */
int tld_email_next(struct tld_email_list *list, struct tld_email *e)
{
    return list->next(e) ? 1 : 0;
}


/* vim: ts=4 sw=4 et
 */
