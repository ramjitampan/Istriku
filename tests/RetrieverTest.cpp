#include "memory/MemoryManager.h"
#include "memory/MemoryRetriever.h"
#include "memory/SQLiteMemoryStorage.h"

#include <iostream>
#include <memory>

using namespace Yuki::Memory;

int main()
{
    auto storage =
        std::make_unique<SQLiteMemoryStorage>();

    MemoryManager manager(
        std::move(storage));

    manager.Remember(
        "user_name",
        "Ramzy");

    MemoryRetriever retriever(manager);

    auto result =
        retriever.Get("user_name");

    if(result)
    {
        std::cout
            << *result
            << '\n';
    }
    else
    {
        std::cout
            << "Not Found\n";
    }

    return 0;
}