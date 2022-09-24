// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// File contains modifications by: The Centure developers
// All modifications:
// Copyright (c) 2017-2022 The Centure developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the Libre Chain License, see the accompanying
// file COPYING
//
// File contains modifications by: The Novo developers
// All modifications:
// Copyright (c) 2020 The Novo developers
// Authored by: Malcolm MacLeod (mmacleod@gmx.com)
// Distributed under the NOVO software license, see the accompanying
// file COPYING

#include <univalue/include/univalue.h>
#include <tinyformat.h>
#include <rpc/server.h>
#include <rpc/client.h>

#include "rpccontroller.h"
#include <boost/algorithm/string/case_conv.hpp>
#include <set>

void RPCController::executeCommandLine(const std::string& sCommandLine, const std::function<void(const std::string&)>& filteredCommandHandler, const std::function<void(const std::string&, const std::string&)>& errorHandler, const std::function<void(const std::string&, const std::string&)>& successHandler)
{
    if(!sCommandLine.empty())
    {
        std::string strFilteredCmd;
        try
        {
            std::string dummy;
            if (!parseCommandLine(dummy, sCommandLine, false, &strFilteredCmd))
            {
                // Failed to parse command, so we cannot even filter it for the history
                errorHandler(strFilteredCmd, "Invalid command line");
                return;
            }
        }
        catch (const std::exception& e)
        {
            errorHandler(strFilteredCmd, strprintf("Error: %s", e.what()));
            return;
        }
        filteredCommandHandler(strFilteredCmd);

        std::string result;
        try
        {
            std::string executableCommand = sCommandLine + "\n";
            parseCommandLine(result, executableCommand, true, NULL);
        }
        catch (UniValue& objError)
        {
            try // Nice formatting for standard-format error
            {
                int code = find_value(objError, "code").get_int();
                std::string message = find_value(objError, "message").get_str();
                errorHandler(strFilteredCmd, strprintf("%s (code %d)", message, code));
                return;
            }
            catch (const std::runtime_error&) // raised when converting to invalid type, i.e. missing code or message
            {   // Show raw JSON object
                //Q_EMIT reply(RPCConsole::CMD_ERROR, QString::fromStdString(objError.write()));
                errorHandler(strFilteredCmd, objError.write());
                return;
            }
        }
        catch (const std::exception& e)
        {
            errorHandler(strFilteredCmd, strprintf("Error: %s", e.what()));
            return;
        }
        successHandler(strFilteredCmd, result);
        return;
    }
    errorHandler("", "Empty commandline");
}

std::vector<std::string> RPCController::getAutocompleteList()
{
    std::vector<std::string> wordList;
    std::vector<std::string> commandList = tableRPC.listCommands();
    for (size_t i = 0; i < commandList.size(); ++i)
    {
        wordList.push_back(commandList[i].c_str());
        wordList.push_back("help " + commandList[i]);
    }

    std::sort(wordList.begin(), wordList.end());
    return wordList;
}

//fixme: (NOVO) - improve this list, e.g. command to import witness keys
// don't add private key handling cmd's to the history
const std::set<std::string> historyFilter = {
    "importprivkey",
    "importmulti",
    "signmessagewithprivkey",
    "signrawtransaction",
    "walletpassphrase",
    "walletpassphrasechange",
    "encryptwallet"
};

bool RPCController::parseCommandLine(std::string& strResult, const std::string& strCommand, const bool fExecute, std::string* const pstrFilteredOut)
{
    std::vector< std::vector<std::string> > stack;
    stack.push_back(std::vector<std::string>());

    enum CmdParseState
    {
        STATE_EATING_SPACES,
        STATE_EATING_SPACES_IN_ARG,
        STATE_EATING_SPACES_IN_BRACKETS,
        STATE_ARGUMENT,
        STATE_SINGLEQUOTED,
        STATE_DOUBLEQUOTED,
        STATE_ESCAPE_OUTER,
        STATE_ESCAPE_DOUBLEQUOTED,
        STATE_COMMAND_EXECUTED,
        STATE_COMMAND_EXECUTED_INNER
    } state = STATE_EATING_SPACES;
    std::string curarg;
    UniValue lastResult;
    unsigned nDepthInsideSensitive = 0;
    size_t filter_begin_pos = 0, chpos;
    std::vector<std::pair<size_t, size_t>> filter_ranges;

    auto add_to_current_stack = [&](const std::string& strArg)
    {
        std::string argLower = strArg;
        boost::to_lower(argLower);
        if (!stack.empty() && stack.back().empty() && (!nDepthInsideSensitive) && historyFilter.find(argLower) != historyFilter.end())
        {
            nDepthInsideSensitive = 1;
            filter_begin_pos = chpos;
        }
        // Make sure stack is not empty before adding something
        if (stack.empty()) {
            stack.push_back(std::vector<std::string>());
        }
        stack.back().push_back(strArg);
    };

    auto close_out_params = [&]()
    {
        if (nDepthInsideSensitive) {
            if (!--nDepthInsideSensitive) {
                assert(filter_begin_pos);
                filter_ranges.push_back(std::pair(filter_begin_pos, chpos));
                filter_begin_pos = 0;
            }
        }
        stack.pop_back();
    };

    std::string strCommandTerminated = strCommand;
    if (strCommandTerminated.back() != '\n')
        strCommandTerminated += "\n";
    for (chpos = 0; chpos < strCommandTerminated.size(); ++chpos)
    {
        char ch = strCommandTerminated[chpos];
        switch(state)
        {
            case STATE_COMMAND_EXECUTED_INNER:
            case STATE_COMMAND_EXECUTED:
            {
                bool breakParsing = true;
                switch(ch)
                {
                    case '[': curarg.clear(); state = STATE_COMMAND_EXECUTED_INNER; break;
                    default:
                        if (state == STATE_COMMAND_EXECUTED_INNER)
                        {
                            if (ch != ']')
                            {
                                // append char to the current argument (which is also used for the query command)
                                curarg += ch;
                                break;
                            }
                            if (curarg.size() && fExecute)
                            {
                                // if we have a value query, query arrays with index and objects with a string key
                                UniValue subelement;
                                if (lastResult.isArray())
                                {
                                    for(char argch: curarg)
                                        if (!std::isdigit(argch))
                                            throw std::runtime_error("Invalid result query");
                                    subelement = lastResult[atoi(curarg.c_str())];
                                }
                                else if (lastResult.isObject())
                                    subelement = find_value(lastResult, curarg);
                                else
                                    throw std::runtime_error("Invalid result query"); //no array or object: abort
                                lastResult = subelement;
                            }

                            state = STATE_COMMAND_EXECUTED;
                            break;
                        }
                        // don't break parsing when the char is required for the next argument
                        breakParsing = false;

                        // pop the stack and return the result to the current command arguments
                        close_out_params();

                        // don't stringify the json in case of a string to avoid doublequotes
                        if (lastResult.isStr())
                            curarg = lastResult.get_str();
                        else
                            curarg = lastResult.write(2);

                        // if we have a non empty result, use it as stack argument otherwise as general result
                        if (curarg.size())
                        {
                            if (stack.size())
                                add_to_current_stack(curarg);
                            else
                                strResult = curarg;
                        }
                        curarg.clear();
                        // assume eating space state
                        state = STATE_EATING_SPACES;
                }
                if (breakParsing)
                    break;
            }
            case STATE_ARGUMENT: // In or after argument
            case STATE_EATING_SPACES_IN_ARG:
            case STATE_EATING_SPACES_IN_BRACKETS:
            case STATE_EATING_SPACES: // Handle runs of whitespace
                switch(ch)
            {
                case '"': state = STATE_DOUBLEQUOTED; break;
                case '\'': state = STATE_SINGLEQUOTED; break;
                case '\\': state = STATE_ESCAPE_OUTER; break;
                case '(': case ')': case '\n':
                    if (state == STATE_EATING_SPACES_IN_ARG)
                        throw std::runtime_error("Invalid Syntax");
                    if (state == STATE_ARGUMENT)
                    {
                        if (ch == '(' && stack.size() && stack.back().size() > 0)
                        {
                            if (nDepthInsideSensitive) {
                                ++nDepthInsideSensitive;
                            }
                            stack.push_back(std::vector<std::string>());
                        }

                        // don't allow commands after executed commands on baselevel
                        if (!stack.size())
                            throw std::runtime_error("Invalid Syntax");

                        add_to_current_stack(curarg);
                        curarg.clear();
                        state = STATE_EATING_SPACES_IN_BRACKETS;
                    }
                    if ((ch == ')' || ch == '\n') && stack.size() > 0)
                    {
                        if (fExecute) {
                            // Convert argument list to JSON objects in method-dependent way,
                            // and pass it along with the method name to the dispatcher.
                            JSONRPCRequest req;
                            req.params = RPCConvertValues(stack.back()[0], std::vector<std::string>(stack.back().begin() + 1, stack.back().end()));
                            req.strMethod = stack.back()[0];
                            lastResult = tableRPC.execute(req);
                        }

                        state = STATE_COMMAND_EXECUTED;
                        curarg.clear();
                    }
                    break;
                case ' ': case ',': case '\t':
                    if(state == STATE_EATING_SPACES_IN_ARG && curarg.empty() && ch == ',')
                        throw std::runtime_error("Invalid Syntax");

                    else if(state == STATE_ARGUMENT) // Space ends argument
                    {
                        add_to_current_stack(curarg);
                        curarg.clear();
                    }
                    if ((state == STATE_EATING_SPACES_IN_BRACKETS || state == STATE_ARGUMENT) && ch == ',')
                    {
                        state = STATE_EATING_SPACES_IN_ARG;
                        break;
                    }
                    state = STATE_EATING_SPACES;
                    break;
                default: curarg += ch; state = STATE_ARGUMENT;
            }
                break;
            case STATE_SINGLEQUOTED: // Single-quoted string
                switch(ch)
            {
                case '\'': state = STATE_ARGUMENT; break;
                default: curarg += ch;
            }
                break;
            case STATE_DOUBLEQUOTED: // Double-quoted string
                switch(ch)
            {
                case '"': state = STATE_ARGUMENT; break;
                case '\\': state = STATE_ESCAPE_DOUBLEQUOTED; break;
                default: curarg += ch;
            }
                break;
            case STATE_ESCAPE_OUTER: // '\' outside quotes
                curarg += ch; state = STATE_ARGUMENT;
                break;
            case STATE_ESCAPE_DOUBLEQUOTED: // '\' in double-quoted text
                if(ch != '"' && ch != '\\') curarg += '\\'; // keep '\' for everything but the quote and '\' itself
                curarg += ch; state = STATE_DOUBLEQUOTED;
                break;
        }
    }
    if (pstrFilteredOut) {
        if (STATE_COMMAND_EXECUTED == state) {
            assert(!stack.empty());
            close_out_params();
        }
        *pstrFilteredOut = strCommand;
        for (auto i = filter_ranges.rbegin(); i != filter_ranges.rend(); ++i) {
            pstrFilteredOut->replace(i->first, i->second - i->first, "(…)");
        }
    }
    switch(state) // final state
    {
        case STATE_COMMAND_EXECUTED:
            if (lastResult.isStr())
                strResult = lastResult.get_str();
            else
                strResult = lastResult.write(2);
        case STATE_ARGUMENT:
        case STATE_EATING_SPACES:
            return true;
        // ERROR to end in one of the other states
        case STATE_EATING_SPACES_IN_ARG: case STATE_EATING_SPACES_IN_BRACKETS: case STATE_SINGLEQUOTED: case STATE_DOUBLEQUOTED: case STATE_ESCAPE_OUTER: case STATE_ESCAPE_DOUBLEQUOTED: case STATE_COMMAND_EXECUTED_INNER:
        default:
            return false;
    }
}
