#pragma once
#include "Arduino.h"
constexpr int MSBFIRST=1, SPI_MODE0=0;
struct SPISettings { SPISettings(unsigned, int, int) {} };
// W5100 and W5500 register transports, not a TCP/IP or electrical model.
struct SPIClass {
    uint8_t regs[65536] = {};
    bool active=false, selected=false, stuckCommand=false, changingRx=false, changingTx=false;
    unsigned pos=0, resets=0, accesses=0;
    uint8_t op=0;
    uint16_t addr=0;
    uint8_t model=51, control=0;
    unsigned phyAssertions=0, phyReleases=0;
    bool ignorePhyWrites=false;
    uint16_t socketBase() const { return model==55 ? 0x1000 : 0x400; }
    uint8_t access(uint16_t a, bool writing, uint8_t v) {
        if(writing) {
            if(a==0 && v==0x80) {
                // Model MR reset preserving PHY state: explicit PHY reset is needed.
                auto phy=regs[0x2e]; std::memset(regs,0,sizeof(regs));
                regs[0x2e]=phy; resets++;
            } else if(model==55 && a==0x2e) {
                if(!ignorePhyWrites) {
                    if(!(v&0x80)) phyAssertions++;
                    else if(!(regs[a]&0x80)) phyReleases++;
                    regs[a]=v;
                }
            } else if(a==socketBase()+1) regs[a]=stuckCommand ? v : 0;
            else regs[a]=v;
            return 0;
        }
        if(model==55 && a==0x39) return 4;
        if ((a==socketBase()+0x26 && changingRx) || (a==socketBase()+0x20 && changingTx)) regs[a+1]^=1;
        return regs[a];
    }
    void begin() {}
    void beginTransaction(const SPISettings&) {
        if (active) throw std::runtime_error("Nested Ethernet SPI transaction");
        active=true;
    }
    void endTransaction() {
        if (!active || selected) throw std::runtime_error("Unbalanced Ethernet SPI transaction");
        active=false;
    }
    uint8_t transfer(uint8_t v) {
        if (!active || !selected) throw std::runtime_error("SPI outside transaction/CS");
        clockUs += 10; accesses++;
        if(model==55) {
            switch(pos++) {
                case 0: addr=uint16_t(v)<<8; return 0;
                case 1: addr|=v; return 0;
                case 2: control=v; return 0;
                default: {
                    // Only variable-data mode is used by this driver. Other
                    // headers (e.g. W5200 probing) are ignored by this double.
                    if(control&3) return 0;
                    uint16_t a=addr++;
                    unsigned block=control>>3;
                    if(block) {
                        unsigned socket=(block-1)/4, region=(block-1)%4;
                        if(region==0) a=0x1000+socket*0x100+(a&0xff);
                        else if(region==1) a=0x8000+socket*0x800+(a&0x7ff);
                        else if(region==2) a=0xc000+socket*0x800+(a&0x7ff);
                        else return 0;
                    }
                    return access(a,(control&4)!=0,v);
                }
            }
        }
        switch(pos++) {
            case 0: op=v; break;
            case 1: addr=uint16_t(v)<<8; break;
            case 2: addr|=v; break;
            case 3:
                if(op==0xF0 || op==0x0F) return access(addr,op==0xF0,v);
                break;
        }
        return 0;
    }
    void transfer(void* data,size_t n) { auto p=static_cast<uint8_t*>(data); for(size_t i=0;i<n;++i) p[i]=transfer(p[i]); }
};
inline SPIClass SPI;
inline void digitalWrite(int pin, int level) {
    if(pin!=10) throw std::runtime_error("Ethernet accessed a non-Ethernet GPIO");
    SPI.selected=level==LOW; SPI.pos=0;
}
