#include "compile_time_json.hpp"

template <auto MyJson>
void compile_time_test_my_json()
{
    static_assert(MyJson["e_12"_member].template get<0>().value == 12345);
    static_assert(MyJson["e_12sdfsdf"_member]["e_"_member].value);
    static_assert(!MyJson["e_12sdfsdf"_member]["e_s"_member].value);
    static_assert(MyJson["e_12sdfsdf"_member]["e_sdf"_member].value == -22);
    static_assert(MyJson["e_12sdfsdf"_member]["list"_member].template get<3>().value == 12354.1234);
}

int main()
{
    auto json_1 = R"(
    {
        "e_12":  [12345.12345],
        "e_12s":  .11,
        "e_12sd":  1.1,
        "e_12sdf":  11.,
        "e_12sdfs":  00,
        "e_12sdfsd":  0.,
        "e_12sdfsdf":
        {
            "e_":  true,
            "e_s":  false,
            "e_sd":  null,
            "e_sdf":  22.,
            "list":  [{}, [], [{"list1":[]}],],
            "e_sdfsd":  {
                "e_sdfsd1": "dsfsdfsdf \n\"\\"
            },
        },
    }
    )"_json;

    std::cout << json_1.get<"e_12"_member>().get<0>().value << std::endl;

    json_1.get<"e_12"_member>().get<0>().value = 12;

    std::cout << json_1.get<"e_12"_member>().get<0>().value << std::endl;

    compile_time_test_my_json<R"(
    {
        "e_12":  [12345],
        "e_12s":  .11,
        "e_12sd":  1.1,
        "e_12sdf":  11.,
        "e_12sdfs":  00,
        "e_12sdfsd":  0.,
        "e_12sdfsdf":
        {
            "e_":  true,
            "e_s":  false,
            "e_sd":  null,
            "e_sdf":  -22,
            "list":  [{}, [], [{"list1":[]}], 12354.1234],
            "e_sdfsd":  {
                "e_sdfsd1": 1235
            },
        },
    }
    )"_json>();
}