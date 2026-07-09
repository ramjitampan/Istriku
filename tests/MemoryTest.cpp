#include "memory/SQLiteMemoryStorage.h"

#include <iostream>

using namespace Yuki::Memory;

int main()
{
    SQLiteMemoryStorage storage;

    std::cout << "===== Memory Test =====\n\n";

    //-----------------------------------
    // Save
    //-----------------------------------
    std::cout << "Saving...\n";

    storage.Save({"name", "Ramzy"});
    storage.Save({"city", "Binjai"});
    storage.Save({"language", "C++"});

    //-----------------------------------
    // Load
    //-----------------------------------
    std::cout << "\nLoading...\n";

    auto name = storage.Load("name");

    if (name)
    {
        std::cout
            << name->key
            << " = "
            << name->value
            << '\n';
    }

    //-----------------------------------
    // Load All
    //-----------------------------------
    std::cout << "\nAll Memories\n";

    auto all = storage.LoadAll();

    for (const auto& entry : all)
    {
        std::cout
            << entry.key
            << " = "
            << entry.value
            << '\n';
    }

    //-----------------------------------
    // Remove
    //-----------------------------------
    std::cout << "\nRemoving city...\n";

    storage.Remove("city");

    //-----------------------------------
    // Check Remove
    //-----------------------------------
    auto city = storage.Load("city");

    if (!city)
    {
        std::cout
            << "city removed successfully.\n";
    }

    //-----------------------------------
    // Clear
    //-----------------------------------
    std::cout
        << "\nClearing database...\n";

    storage.Clear();

    //-----------------------------------
    // Verify Empty
    //-----------------------------------
    if (storage.LoadAll().empty())
    {
        std::cout
            << "Database is empty.\n";
    }

    std::cout
        << "\n===== TEST FINISHED =====\n";

    return 0;
}