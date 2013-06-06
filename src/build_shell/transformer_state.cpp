/*
 * Copyright © 2013 Jørgen Lind

 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.

 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
*/

#include "transformer_state.h"


void TransformerState::updateState(const JT::Token &token)
{
    if (token.value_type == JT::Token::ObjectStart) {
        depth++;
        if (depth == 2) {
            if (token.name_type == JT::Token::String) {
                current_project = std::string(token.name.data + 1, token.name.size - 2);
            } else {
                current_project = std::string(token.name.data, token.name.size);
            }
        }
    } else if (token.value_type == JT::Token::ObjectEnd) {
        depth--;
    }
}

void TransformerState::cacheAndExpandToken(const JT::Token &token)
{
    token_value = std::string(token.value.data, token.value.size);
    token_value = build_environment.expandVariablesInString(token_value, current_project);
    temp_token = token;
    temp_token.value.data = token_value.c_str();
    temp_token.value.size = token_value.size();
}

