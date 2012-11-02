#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iostream>

#include <QByteArray>
#include <QList>

#define DBG(X) do {std::cerr << X << " (current char: " << *p << ")" << std::endl;} while(false);


%%{
    machine rfc5322;

    action push_current_char {
        std::cerr << "push_current_char " << *p << std::endl;
        str.append(*p);
    }

    action clear_str {
        std::cerr << "clear_str" << std::endl;
        str.clear();
    }

    action push_current_backslashed {
        switch (*p) {
            case 'n':
                str += "\n";
                break;
            case 'r':
                str += "\r";
                break;
            case '0':
                str += "\0";
                break;
            case '\\':
                str += "\\";
                break;
            default:
                str += '\\' + *p;
        }
        std::cerr << "push_current_backslashed \\" << *p << std::endl;
    }

    action push_string_list {
        std::cerr << "push_string_list " << str.data() << std::endl;
        list.append(str);
        str.clear();
    }

    action clear_list {
        std::cerr << "clear_list" << std::endl;
        list.clear();
        str.clear();
    }

    action got_message_id_header {
        if (list.size() == 1) {
            std::cout << "Message-Id: " << list[0].data() << std::endl;
        } else {
            std::cout << "invalid Message-Id" << std::endl;
        }
    }

    action got_in_reply_to_header {
        std::cout << "In-Reply-To: ";
        Q_FOREACH(const QByteArray &item, list) {
            std::cout << "<" << item.data() << ">, ";
        }
        std::cout << std::endl;
    }

    action got_references_header {
        std::cout << "References: ";
        Q_FOREACH(const QByteArray &item, list) {
            std::cout << "<" << item.data() << ">, ";
        }
        std::cout << std::endl;
    }

    action dbg_phrase {
        std::cerr << "dbg_phrase" << std::endl;
    }



    include rfc5322 "rfc5322.rl";

    main := (optional_field | references)* CRLF*;

    write data;
}%%

int main()
{
    QByteArray data;
    std::string line;

    while (std::getline(std::cin, line)) {
        data += line.c_str();
        data += '\n';
    }

    const char *p = data.data();
    const char *pe = p + data.length();
    const char *eof = pe;
    int cs;

    const char *orig = p;
    int start_string_offset = 0;
    QByteArray str;
    QList<QByteArray> list;

    %% write init;
    %% write exec;

    std::cout << "cs: " << cs << std::endl;
    return 0;
}
