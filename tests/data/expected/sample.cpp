#include <iostream>
#include <string>
#include <vector>

// A simple class
class Person {
private:
    std::string name;
    int age;

public:
    Person(const std::string& n, int a) : name(n), age(a) {}

    void sayHello() const {
        std::cout << "Hi, I'm " << name << " and I'm " << age << " years old.\n";
    }
};

// A function that prints a list
void printNumbers(const std::vector<int>& nums) {
    std::cout << "Numbers: ";
    for (int n : nums) {
        std::cout << n << " ";
    }
    std::cout << "\n";
}

int main() {
    // New: user input
    std::string userName;
    int userAge;

    std::cout << "Enter your name: ";
    std::getline(std::cin, userName);

    std::cout << "Enter your age: ";
    std::cin >> userAge;

    Person user(userName, userAge);
    user.sayHello();

    // Existing sample data
    Person alice("Alice", 30);
    alice.sayHello();

    std::vector<int> numbers = {10, 20, 30};
    printNumbers(numbers);

    // Modified loop (counting backward)
    std::cout << "Countdown: ";
    for (int i = 5; i >= 1; i--) {
        std::cout << i << " ";
    }
    std::cout << "\n";

    return 0;
}
