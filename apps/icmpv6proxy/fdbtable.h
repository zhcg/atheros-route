/**
*   Fdb table structure definition. Double linked list...
*/

#define FDBSTATE_NOTJOINED            0   // The group corresponding to route is not joined
#define FDBSTATE_JOINED               1   // The group corresponding to route is joined
#define FDBSTATE_CHECK_LAST_MEMBER    2   // The router is checking for hosts

struct FdbTable {
    struct FdbTable *nextfdb;   // Pointer to the next group in line.
    struct FdbTable *prevfdb;   // Pointer to the previous group in line.
    struct in6_addr  group;      // The group to route
    uint32           swifBits;   // Bits representing receiving switch ports

    // Keeps the upstream membership state...
    short upstrState;           // Upstream membership state.

    // These parameters contain aging details for igmp snooping.
    uint32 ageSwifBits;         // Bits representing aging switch ports - Ports received report
    uint32 ageSwifResultBits;    // Bits representing aging Ports result.

    int ageValue;             // Downcounter for death.          
    int ageActivity;          // Records any acitivity that notes there are still listeners.
};

void clearAllFdbs();
void ageActiveFdbs();
int internUpdateFdb(struct in6_addr *group, uint32 ifBits, int activate);
void logFdbTable(char *);
int insertFdb(struct in6_addr *group, uint32 port);
