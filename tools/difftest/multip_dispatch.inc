/* Drive the recovered handlers through their actual dispatch owner, including
 * deterministic caller-stack leftovers consumed by weather and position. */
static const struct multip_module *s_dispatch_module;
typedef int32_t (STDCALL *dispatch_packet_fn)(uint32_t, const void *, uint32_t, uint32_t);
__attribute__((naked)) static int32_t dispatch_painted(void *function __attribute__((unused)),
    uint32_t id __attribute__((unused)), const void *packet __attribute__((unused)),
    uint32_t size __attribute__((unused)), uint32_t timestamp __attribute__((unused)))
{
    __asm__ volatile("pushl %ebx\n\tpushl %esi\n\tpushl %edi\n\tmovl %esp, %ebx\n\t"
        "subl $0x1000, %esp\n\tmovl %esp, %edi\n\tmovl $1024, %ecx\n\t"
        "movl $0x6a39c751, %eax\n\tcld\n\trep stosl\n\taddl $0xf00, %esp\n\t"
        "pushl 32(%ebx)\n\tpushl 28(%ebx)\n\tpushl 24(%ebx)\n\tpushl 20(%ebx)\n\t"
        "call *16(%ebx)\n\taddl $0x100, %esp\n\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tret");
}
static int32_t dispatch_route(uint32_t tag, uint32_t id, const void *packet, uint32_t size, uint32_t timestamp)
{
    uint32_t empty[4] = {0}, saved;
    int32_t result;
    if (packet == NULL) packet = empty;
    saved = get_u32(packet, 0);
    put_u32((void *)packet, 0, tag);
    result = dispatch_painted(send_internal(s_dispatch_module, 217), id, packet, size, timestamp);
    put_u32((void *)packet, 0, saved);
    return result;
}
#define DISPATCH_ROUTE(tag) \
static int32_t STDCALL dispatch_route_##tag(uint32_t id, const void *packet, uint32_t size) \
{ return dispatch_route(0x##tag, id, packet, size, 0); }
DISPATCH_ROUTE(1000)
DISPATCH_ROUTE(1001)
DISPATCH_ROUTE(1002)
DISPATCH_ROUTE(1003)
DISPATCH_ROUTE(1004)
DISPATCH_ROUTE(1007)
DISPATCH_ROUTE(1008)
DISPATCH_ROUTE(1009)
DISPATCH_ROUTE(100a)
DISPATCH_ROUTE(100c)
DISPATCH_ROUTE(100d)
DISPATCH_ROUTE(100e)
DISPATCH_ROUTE(100f)
DISPATCH_ROUTE(1010)
DISPATCH_ROUTE(1011)
DISPATCH_ROUTE(1012)
DISPATCH_ROUTE(1013)
DISPATCH_ROUTE(1014)
DISPATCH_ROUTE(1015)
DISPATCH_ROUTE(1018)
DISPATCH_ROUTE(1019)
DISPATCH_ROUTE(101a)
DISPATCH_ROUTE(101b)
DISPATCH_ROUTE(101c)
DISPATCH_ROUTE(101d)
DISPATCH_ROUTE(101e)
DISPATCH_ROUTE(101f)
static int32_t STDCALL dispatch_route_1017(uint32_t id, const void *packet, uint32_t size, uint32_t timestamp)
{ return dispatch_route(0x1017, id, packet, size, timestamp); }
static int32_t dispatch_system_route(uint32_t tag, const void *packet, uint32_t size, uint32_t timestamp)
{
    typedef int32_t (STDCALL *system_dispatch_fn)(const void *, uint32_t, uint32_t);
    uint32_t saved = get_u32(packet, 0);
    int32_t result;
    put_u32((void *)packet, 0, tag);
    result = ((system_dispatch_fn)send_internal(s_dispatch_module, 216))(packet, size, timestamp);
    put_u32((void *)packet, 0, saved);
    return result;
}
#define DISPATCH_SYSTEM_ROUTE(name, tag) \
static int32_t STDCALL dispatch_system_##name(const void *packet, uint32_t size) \
{ return dispatch_system_route(tag, packet, size, 0); }
DISPATCH_SYSTEM_ROUTE(created, 3)
DISPATCH_SYSTEM_ROUTE(destroyed, 5)
DISPATCH_SYSTEM_ROUTE(host, 0x101)
static int32_t STDCALL dispatch_system_data(const void *packet, uint32_t size, uint32_t timestamp)
{ return dispatch_system_route(0x102, packet, size, timestamp); }
static void *dispatch_entry(const struct multip_module *module, uint32_t index)
{
    s_dispatch_module = module;
    switch (index) {
    case 116: return dispatch_system_created;
    case 117: return dispatch_system_destroyed;
    case 118: return dispatch_system_data;
    case 119: return dispatch_system_host;
    case 120: return dispatch_route_1014;
    case 121: return dispatch_route_1013;
    case 122: return dispatch_route_1001;
    case 123: return dispatch_route_1002;
    case 124: return dispatch_route_1008;
    case 125: return dispatch_route_1009;
    case 126: return dispatch_route_1003;
    case 127: return dispatch_route_1004;
    case 128: return dispatch_route_1007;
    case 129: return dispatch_route_100a;
    case 130: return dispatch_route_100c;
    case 131: return dispatch_route_100d;
    case 132: return dispatch_route_100f;
    case 133: return dispatch_route_1011;
    case 134: return dispatch_route_1012;
    case 136: return dispatch_route_1000;
    case 138: return dispatch_route_1018;
    case 139: return dispatch_route_1019;
    case 140: return dispatch_route_1017;
    case 160: return dispatch_route_101d;
    case 161: return dispatch_route_101e;
    case 162: return dispatch_route_101f;
    case 177: return dispatch_route_1015;
    case 185: return dispatch_route_1010;
    case 192: return dispatch_route_101a;
    case 193: return dispatch_route_101b;
    case 194: return dispatch_route_101c;
    case 196: return dispatch_route_100e;
    default: return NULL;
    }
}
static uint32_t test_dispatch(const struct multip_module *retail, const struct multip_module *ours, uint32_t scenarios)
{
    uint32_t bad = 0;
    s_dispatch_routes = 1;
    bad += test_app_players(retail, ours, scenarios);
    bad += test_updates(retail, ours, scenarios);
    bad += test_tick(retail, ours, scenarios);
    bad += test_system(retail, ours, scenarios);
    bad += test_texture_receive(retail, ours, scenarios);
    bad += test_model_receive(retail, ours, scenarios);
    bad += test_title_receive(retail, ours, scenarios);
    bad += test_model_send(retail, ours, scenarios);
    s_dispatch_routes = 0;
    printf("MULTIP application receive dispatch: %lu scenarios, %lu differ\n",
        (unsigned long)scenarios, (unsigned long)bad);
    return bad;
}
