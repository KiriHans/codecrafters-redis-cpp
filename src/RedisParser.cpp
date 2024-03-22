#include <iostream>
#include <string>
#include <vector>
#include <tuple>
#include <unordered_map>

#include <chrono>


struct RESPBulkString
{
    std::string value;
};

struct RESPArray
{
    std::vector<std::string> array;
};

struct RESP
{
    char type;
    std::string raw;
    std::string data;
    std::vector<std::string> array;
    int count;
};

enum RESPMessageType
{
    BULK_STRING,
    ARRAY,
};

class RedisParser
{
    static std::tuple<size_t, RESP> parseBulkyString(RESP &parsed_message, const std::string_view &unparsed_message, int i)
    {
        size_t count = std::stoi(parsed_message.data);

        if (unparsed_message.substr(count + 1, 1) != "\n" || unparsed_message.substr(count, 1) != "\r")
        {
            return {0, {}};
        }

        parsed_message.raw = unparsed_message.substr(0, count + 2);
        parsed_message.data = unparsed_message.substr(0, count);
        parsed_message.count = parsed_message.raw.size() + i;

        // std::cout << "unparsed_message 2: " << unparsed_message << ": " << parsed_message.raw.size() << ": "<< count << std::endl;

        return {parsed_message.count, parsed_message};
    }

public:
    static std::unordered_map<std::string, std::string> store;
    static std::unordered_map<std::string, std::chrono::steady_clock::time_point> expireKeys;

    static std::tuple<size_t, RESP> parse(const std::string_view &message, RESP parsed_message = {})
    {
        char message_type = message[0];

        size_t i = 1;
        for (;; i++)
        {
            if (message.size() == i)
            {
                return {0, {}};
            }
            if (message[i] == '\n')
            {
                if (message[i - 1] != '\r')
                {
                    return {0, {}};
                }
                i++;
                break;
            }
        }

        parsed_message.raw = message.substr(0, i);
        parsed_message.data = message.substr(1, i - 2);

        std::vector<std::string> data_array = {};
        std::string_view unparsed_message;
        int total_count;
        size_t number_elements;

        switch (message_type)
        {
        case '+':
            return {parsed_message.raw.size(), parsed_message};
        case '$':
            parsed_message.type = BULK_STRING;

            unparsed_message = message.substr(i);
            return parseBulkyString(parsed_message, unparsed_message, i);
            break;
        case '*':
            parsed_message.type = ARRAY;

            number_elements = std::stoi(parsed_message.data);
            unparsed_message = message.substr(i);


            parsed_message.raw = unparsed_message;

            total_count = 0;

            for (size_t i = 0; i < number_elements; i++)
            {


                auto [count, new_message] = parse(unparsed_message, parsed_message);
                total_count += count;
                unparsed_message = unparsed_message.substr(count);

                data_array.push_back(new_message.data);
            };

            parsed_message.array = data_array;
            parsed_message.raw = parsed_message.raw.substr(0, i + total_count);
           
            
            // std::cout << "Wcho " << message << std::endl;

            return {parsed_message.raw.size(), parsed_message};
        default:
            return {0, {}};
        }
    }
};