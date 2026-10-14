#include <iostream>
#include <string>
#include "Arduino.h"
#include "SPI.h"
// Expose register/socket helpers only in this test translation unit.
#define private public
#include "../../Software/lib/Ethernet/src/Ethernet.h"
#include "../../Software/lib/Ethernet/src/utility/w5100.h"
#undef private
#include "../../Software/lib/Ethernet/src/utility/w5100.cpp"
#include "../../Software/lib/Ethernet/src/socket.cpp"
uint16_t EthernetServer::server_port[MAX_SOCK_NUM] = {};
bool fault=false;
uint32_t cancelAt=0xFFFFFFFFu;
bool dacNetworkCancelled() { return fault || millis()>=cancelAt; }
void dacNetworkFault() { fault=true; }
void require(bool v,const char* msg) { if(!v) throw std::runtime_error(msg); }
void run(const std::string& id, uint8_t model) {
    SPI=SPIClass(); SPI.model=model;
    clockUs=0; fault=false; cancelAt=0xFFFFFFFFu;
    EthernetClass::resetSockets();
    W5100.chip=model; W5100.CH_BASE_MSB=model==55 ? 0x10 : 4; W5100.initialized=true;
    auto base=SPI.socketBase();
    SPI.regs[base+3]=SnSR::ESTABLISHED;
    SPI.regs[base+0x20]=8; // 2048 bytes TX space.
    uint8_t data=42;
    if(id=="N1" || id=="N2") {
        SPI.stuckCommand=true;
        if(id=="N2") cancelAt=25;
        SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
        W5100.execCmdSn(0,Sock_SEND);
        SPI.endTransaction();
        require(id=="N1" ? fault : !fault, "Socket command wait did not timeout/cancel correctly");
        require(millis()<=101, "Socket command wait exceeded bound");
    } else if(id=="N3" || id=="N4") {
        SPI.changingRx=id=="N3"; SPI.changingTx=id=="N4";
        SPI.beginTransaction(SPI_ETHERNET_SETTINGS);
        auto n=id=="N3" ? getSnRX_RSR(0) : getSnTX_FSR(0);
        SPI.endTransaction();
        require(n==0 && fault && millis()<=101, "Unstable size register did not time out");
    } else if(id=="N5" || id=="N6" || id=="N7") {
        if(id=="N5") SPI.regs[base+0x20]=0;
        if(id=="N7") cancelAt=25;
        auto n=EthernetClass::socketSend(0,&data,1);
        require(n==0 && dacNetworkCancelled() && millis()<=102, "TCP wait did not terminate");
    } else if(id=="N8") {
        W5100.invalidate();
        require(!W5100.initialized && W5100.chip==0, "Reset retained cached chip state");
        require(W5100.init()==1 && W5100.getChip()==model && SPI.resets>0, "Reinitialization skipped chip reset/detection");
        state[0].RX_RSR=99; state[0].RX_inc=3; EthernetServer::server_port[0]=23;
        EthernetClass::resetSockets();
        require(state[0].RX_RSR==0 && state[0].RX_inc==0 && EthernetServer::server_port[0]==0, "Socket caches survived reset");
    } else if(id=="N9") {
        W5100.invalidate(); cancelAt=25;
        require(W5100.init()==0 && millis()<=26 && SPI.accesses==0, "Button could not cancel startup delay");
    } else if(id=="N10") {
        require(!EthernetClass::socketSendUDP(0) && fault && millis()<=102, "UDP wait did not time out");
    } else if(id=="N11" || id=="N12") {
        SPI.regs[0x2e]=0xf0; // PHY software power-down, unaffected by MR reset.
        SPI.ignorePhyWrites=id=="N12";
        W5100.invalidate();
        auto ok=W5100.init();
        if(id=="N11") {
            require(ok && SPI.phyAssertions==1 && SPI.phyReleases==1 && SPI.regs[0x2e]==0xf8,
                    "W5500 PHY was not reset and restored to auto-negotiation");
        } else {
            require(!ok && fault && !W5100.initialized && W5100.chip==0,
                    "Failed W5500 PHY configuration incorrectly reported ready");
        }
    } else throw std::runtime_error("Unknown network test");
    require(!SPI.active && !SPI.selected,"SPI not released on failure/cancel");
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Supply test ID");
        std::string id=argv[1];
        if(id!="N11" && id!="N12") run(id,51);
        run(id,55);
        std::cout<<"PASS "<<id<<"\n"; return 0;
    }
    catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n"; return 1; }
}
