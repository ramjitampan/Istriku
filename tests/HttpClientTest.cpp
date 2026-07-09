using namespace Yuki::Utils;

int main()
{
    HttpClient client;

    auto response = client.Get(
        "https://httpbin.org/get"
    );

    auto response =
client.Post(
    "https://httpbin.org/post",
    R"({"name":"Ramzy"})"
    );
    auto response2 = client.Get("https://abc.xyz.invalid");



    if(response.success)
    {
        std::cout << "SUCCESS\n";
        std::cout << response.statusCode << '\n';
        std::cout << response.body << '\n';
    }
    else
    {
        std::cout << response.errorMessage;
    }
}
