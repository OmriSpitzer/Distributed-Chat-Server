#include <iostream>
#include <string>
#include <string_view>

int get_answer();
std::string change_value(std::string_view);
void review(std::string_view, std::string_view, std::string_view, std::string_view);
void full_name(std::string_view, std::string_view);
std::string string_to_upper(std::string);

std::string string_to_upper(std::string str)
{
    for (char &c : str)
    {
        c = std::toupper(static_cast<unsigned char>(c));
    }
    return str;
}
void full_name(std::string_view name)
{
    std::cout << "Name: " << name << "\n";
}

void review(std::string_view name, std::string_view phone_num, std::string_view email)
{
    full_name(name);
    std::cout << "Phone: " << phone_num << "\n";
    std::cout << "Email: " << email << "\n\n";
}

std::string change_value(std::string_view type)
{
    bool valid_answer{false};
    std::string answer{};

    do
    {
        std::cout << "Enter " << type << ": ";
        std::getline(std::cin >> std::ws, answer);
        std::cout << "\n";

        if (answer.length() == 0)
            std::cout << "Must enter a character!\n\n";
        else
            valid_answer = true;
    } while (!valid_answer);

    return answer;
}

int get_answer()
{
    std::string answer{};
    int i_answer{};
    bool valid_answer{false};

    do
    {
        std::cout << "========== Contact Manager ==========\n";
        std::cout << "1. Show Contact Information\n";
        std::cout << "2. Change First Name\n";
        std::cout << "3. Change Last Name\n";
        std::cout << "4. Change Phone Number\n";
        std::cout << "5. Change Email Address\n";
        std::cout << "6. Show Full Name\n";
        std::cout << "7. Show Name Length\n";
        std::cout << "8. Convert Full Name to Uppercase\n";
        std::cout << "9. Exit\n";
        std::cout << "=====================================\n";
        std::cout << "Choose an option: ";

        std::getline(std::cin >> std::ws, answer);
        std::cout << "\n";

        if (answer.length() != 1)
        {
            std::cout << "Option must only consist of 1 letter!\n\n";
        }
        else
        {
            try
            {
                i_answer = std::stoi(answer);
                valid_answer = true;
            }
            catch (const std::invalid_argument &)
            {
                std::cout << "Option must be between 1-9!\n\n";
            }
        }
    } while (!valid_answer);

    return i_answer;
}

int main()
{
    int answer{0};
    std::string f_name{""}, l_name{""}, phone_num{""}, email{""};

    while (answer != 9)
    {
        answer = get_answer();
        switch (answer)
        {
        case 1:
            review(f_name + " " + l_name, phone_num, email);
            break;
        case 2:
            f_name = change_value("first name");
            break;
        case 3:
            l_name = change_value("last name");
            break;
        case 4:
            phone_num = change_value("phone number");
            break;
        case 5:
            email = change_value("email");
            break;
        case 6:
            full_name(f_name + " " + l_name);
            std::cout << '\n';
            break;
        case 7:
            std::cout << (f_name + " " + l_name).length() << " Charactars\n\n";
            break;
        case 8:
            f_name = string_to_upper(f_name);
            l_name = string_to_upper(l_name);
            break;
        default:
        std::cout << "Invalid option entered!\n\n";
        }
    }
    return 0;
}