#include <catch2/catch_test_macros.hpp>

#include "maestro/acp/AcpClient.hpp"
#include "maestro/acp/FakeAcpTransport.hpp"
#include "maestro/acp/SessionManager.hpp"

using namespace maestro::acp;

TEST_CASE("sessions register and expose their controllers", "[sessions]") {
    FakeAcpTransport transport;
    AcpClient client(transport);
    SessionManager mgr;

    mgr.add({"nucleus-1", "nucleus", std::nullopt}, client);

    REQUIRE(mgr.size() == 1);
    REQUIRE(mgr.controller("nucleus-1") != nullptr);
    REQUIRE(mgr.controller("missing") == nullptr);
    REQUIRE(mgr.info("nucleus-1")->role == "nucleus");
}

TEST_CASE("childrenOf reflects the nucleus->electron structure in order", "[sessions]") {
    FakeAcpTransport transport;
    AcpClient client(transport);
    SessionManager mgr;

    mgr.add({"nucleus-1", "nucleus", std::nullopt}, client);
    mgr.add({"worker-a", "worker", std::string("nucleus-1")}, client);
    mgr.add({"worker-b", "worker", std::string("nucleus-1")}, client);
    mgr.add({"worker-c", "worker", std::string("other")}, client);

    const auto children = mgr.childrenOf("nucleus-1");
    REQUIRE(children == std::vector<std::string>{"worker-a", "worker-b"});
    REQUIRE(mgr.childrenOf("other") == std::vector<std::string>{"worker-c"});
}

TEST_CASE("remove drops a session and its controller", "[sessions]") {
    FakeAcpTransport transport;
    AcpClient client(transport);
    SessionManager mgr;

    mgr.add({"worker-a", "worker", std::string("nucleus-1")}, client);
    REQUIRE(mgr.size() == 1);

    mgr.remove("worker-a");
    REQUIRE(mgr.size() == 0);
    REQUIRE(mgr.controller("worker-a") == nullptr);
    REQUIRE(mgr.childrenOf("nucleus-1").empty());
}
