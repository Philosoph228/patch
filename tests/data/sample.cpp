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
    // Basic variables
    int value = 42;
    double pi = 3.14159;

    std::cout << "Value: " << value << ", Pi: " << pi << "\n";

    // Working with a vector
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    printNumbers(numbers);

    // Create and use a class
    Person alice("Alice", 30);
    alice.sayHello();

    // Loop example
    std::cout << "Counting: ";
    for (int i = 0; i < 5; i++) {
        std::cout << i << " ";
    }
    std::cout << "\n";

    return 0;
}
