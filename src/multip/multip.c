/* MULTIP.DLL -- module id 35: multiplayer sessions and remote aircraft.
 *
 * In-progress transliteration: transport/workers, DirectPlay lifetime,
 * profiles, session/group primitives, aircraft and player ownership, browser
 * records, tracking/selection, message dispatch and session completion/close.
 * Public lifecycle and session/dialog wiring remain incomplete. MULTIP is
 * intentionally outside MODULES and must not be installed in-game yet.
 *
 * The retail module keeps its runtime state in the uninitialised tail of its
 * .data page.  Keeping a page here, with ImportTable at +0x000 and Linkage at
 * +0x058, lets the differential harness set up that state at the same offsets.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"

#ifndef FS98_STDCALL
#define FS98_STDCALL __attribute__((stdcall))
#endif

#define MULTIP_IMAGE_SIZE       0x1000
#define MULTIP_LINKAGE          0x0058
#define MULTIP_PLAYER_SIZE      0x0100

#define M_LOCAL_PLAYER          0x0a3c
#define M_DIRECTPLAY_LOADED     0x0a48
#define M_PLAYER_MODE           0x0a5c
#define M_ACTIVE_PLAYER_ID      0x0a60
#define M_CONNECTED             0x0aac
#define M_AIRCRAFT_ACTIVE       0x0ab4
#define M_PLAYER_COUNT          0x0660
#define M_PLAYERS               0x0678
#define M_PLAYERS_RESULT        0x0ef8

#define IMPORT_HOST             0x0004
#define M_DIRECTPLAY_EXPORTS    0x0148
#define M_DIRECTPLAY_LOBBY      0x0980
#define M_DIRECTPLAY_CREATE     0x098c
#define M_DIRECTPLAY_ENUMERATE  0x0990
#define M_DIRECTPLAY_QUERIED    0x0a44
#define M_DIRECTPLAY            0x0988
#define M_RECEIVE_STOP          0x0df0
#define M_RECEIVE_EVENT         0x0df4
#define M_RECEIVE_THREAD        0x0dfc
#define M_RECEIVE_THREAD_ID     0x0e00
#define M_RECEIVE_RUNNING       0x0e04
#define M_SEND_STOP             0x0e58
#define M_SEND_THREAD           0x0e68
#define M_SEND_THREAD_ID        0x0e6c
#define M_SEND_RUNNING          0x0e70
#define M_RECEIVE_QUEUE         0x0df8
#define M_SEND_EVENT            0x0e5c
#define M_SEND_QUEUE_FLAGS      0x0e60
#define M_SEND_QUEUE_PLAIN      0x0e64
#define M_WORKERS_STARTED       0x0f24
#define M_GUARANTEED_MESSAGES   0x0f00

#define IMPORT_GLOBAL           0x000c
#define IMPORT_UTIL             0x0044

struct multip_image {
    uint8_t data[MULTIP_IMAGE_SIZE];
};

static struct multip_image s_multip_image = {{
#include "multip_data.inc"
}};

static uint8_t s_multip_rdata[] = {
#include "multip_rdata.inc"
};

struct multip_data_pointer {
    uint32_t owner;
    uint32_t offset;
    uint32_t kind;
    uint32_t target;
};

static const struct multip_data_pointer s_multip_data_pointers[] = {
#include "multip_pointers.inc"
};

#define D       ((uint32_t)(uintptr_t)s_multip_image.data)
#define M32(o)  (*(volatile uint32_t *)(uintptr_t)(D + (o)))
#define M8(o)   (*(uint8_t *)(uintptr_t)(D + (o)))
#define M16(o)  (*(uint16_t *)(uintptr_t)(D + (o)))
#define MD(o)   (*(double *)(uintptr_t)(D + (o)))
#define ADDR(p) ((uint32_t)(uintptr_t)(p))

__asm__(".globl _ImportTable\n\t.set _ImportTable, _s_multip_image\n\t"
        ".globl _Linkage\n\t.set _Linkage, _s_multip_image + 0x58");

static uint32_t FS98_STDCALL multip_unimplemented(void)
{
    return 0xffffffffu;
}

/* Queue ABI shared with retail's receive and send workers. */
struct multip_queue_node {
    void *record;
    struct multip_queue_node *next;
};

struct multip_queue {
    uint32_t count;
    struct multip_queue_node *head;
    struct multip_queue_node *tail;
    CRITICAL_SECTION lock;
};

struct multip_send_record {
    uint32_t to_id;
    void *payload;
    uint32_t size;
    uint32_t flags;
    uint32_t reserved[4];
};

typedef int32_t (FS98_STDCALL *multip_alloc_fn)(int32_t, uint32_t,
                                               void **, int32_t);
typedef int32_t (FS98_STDCALL *multip_realloc_fn)(int32_t, uint32_t, void **);
typedef uint32_t (FS98_STDCALL *multip_ticks_fn)(void);
typedef int32_t (FS98_STDCALL *multip_dp_receive_fn)(void *, uint32_t *,
    uint32_t *, uint32_t, void *, uint32_t *);
typedef int32_t (FS98_STDCALL *multip_free_fn)(void **);
typedef int32_t (FS98_STDCALL *multip_dp_send_fn)(void *, uint32_t, uint32_t,
                                                uint32_t, void *, uint32_t);
typedef int32_t (FS98_STDCALL *multip_copy_fn)(void *, const void *, uint32_t);

#define HOST_FN(offset, type) (*(type *)(uintptr_t)(M32(IMPORT_HOST) + (offset)))

/* .text+0x4da0 -- append under the queue's critical section. Retail leaves
 * the lock held if the node allocation fails; keep that failure ordering. */
static int32_t multip_queue_append(struct multip_queue *queue, void *record)
{
    struct multip_queue_node *node;

    if (queue == NULL)
        return -1;
    EnterCriticalSection(&queue->lock);
    if (HOST_FN(0x20c, multip_alloc_fn)(1, sizeof(*node),
                                       (void **)&node, 1) != 0)
        return -1;
    node->record = record;
    node->next = NULL;
    if (queue->head == NULL)
        queue->head = node;
    else
        queue->tail->next = node;
    queue->tail = node;
    queue->count++;
    LeaveCriticalSection(&queue->lock);
    return 0;
}

/* .text+0x4e30 -- remove first, then free its node under the lock. */
static int32_t multip_queue_take(struct multip_queue *queue, void **record)
{
    struct multip_queue_node *node;
    int32_t result;

    if (queue == NULL)
        return -1;
    EnterCriticalSection(&queue->lock);
    node = queue->head;
    if (node == NULL) {
        *record = NULL;
        LeaveCriticalSection(&queue->lock);
        return 0;
    }
    *record = node->record;
    queue->head = node->next;
    if (queue->head == NULL)
        queue->tail = NULL;
    queue->count--;
    result = HOST_FN(0x214, multip_free_fn)((void **)&node);
    LeaveCriticalSection(&queue->lock);
    return result != 0 ? -1 : 0;
}

/* .text+0x4f60 -- a missing queue has count zero. */
static uint32_t multip_queue_count(struct multip_queue *queue)
{
    uint32_t count;

    if (queue == NULL)
        return 0;
    EnterCriticalSection(&queue->lock);
    count = queue->count;
    LeaveCriticalSection(&queue->lock);
    return count;
}

/* .text+0x79d0 -- only DPERR_BUSY retries. Other HRESULTs are discarded;
 * the result reports queue/free failures, not the DirectPlay send result. */
int32_t FS98_STDCALL multip_send_one(struct multip_queue *queue)
{
    struct multip_send_record *record;
    uint8_t *directplay;
    uint8_t *local;
    multip_dp_send_fn send;
    int32_t result;

    if (multip_queue_take(queue, (void **)&record) != 0)
        return -1;
    if (record == NULL)
        return 0;
    do {
        directplay = (uint8_t *)(uintptr_t)M32(M_DIRECTPLAY);
        local = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
        send = *(multip_dp_send_fn *)(void *)
            (*(uint8_t **)(void *)directplay + 0x68);
        result = send(directplay, *(uint32_t *)(void *)(local + 4),
                      record->to_id, record->flags, record->payload,
                      record->size);
    } while ((uint32_t)result == 0x8877010eu);
    if (HOST_FN(0x214, multip_free_fn)(&record->payload) != 0)
        return -1;
    return HOST_FN(0x214, multip_free_fn)((void **)&record) != 0 ? -1 : 0;
}

/* .text+0x7970 -- drain plain traffic, then at most one flagged message. */
int32_t multip_send_drain(void)
{
    while (multip_queue_count((struct multip_queue *)(uintptr_t)
                              M32(M_SEND_QUEUE_PLAIN)) != 0)
        if (multip_send_one((struct multip_queue *)(uintptr_t)
                            M32(M_SEND_QUEUE_PLAIN)) != 0)
            return -1;
    return multip_send_one((struct multip_queue *)(uintptr_t)
                           M32(M_SEND_QUEUE_FLAGS)) != 0 ? -1 : 0;
}

/* .text+0x7aa0 -- flush all plain messages before all flagged messages. */
int32_t multip_send_drain_all(void)
{
    uint32_t offset;

    for (offset = M_SEND_QUEUE_PLAIN; ; offset = M_SEND_QUEUE_FLAGS) {
        while (multip_queue_count((struct multip_queue *)(uintptr_t)
                                  M32(offset)) != 0)
            if (multip_send_one((struct multip_queue *)(uintptr_t)
                                M32(offset)) != 0)
                return -1;
        if (offset == M_SEND_QUEUE_FLAGS)
            return 0;
    }
}

struct multip_receive_record {
    uint32_t from_id;
    uint32_t to_id;
    void *payload;
    uint32_t size;
    uint32_t timestamp;
};

/* .text+0x5820 -- receive one message, growing its buffer until it fits.
 * Receive HRESULTs are not propagated; only host/queue failures return -1. */
int32_t FS98_STDCALL multip_receive(uint32_t *received)
{
    struct multip_receive_record *record;
    uint8_t *directplay;
    multip_dp_receive_fn receive;
    int32_t result;

    *received = 0;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, sizeof(*record),
                                       (void **)&record, 1) != 0)
        return -1;
    for (;;) {
        directplay = (uint8_t *)(uintptr_t)M32(M_DIRECTPLAY);
        receive = *(multip_dp_receive_fn *)(void *)
            (*(uint8_t **)(void *)directplay + 0x64);
        result = receive(directplay, &record->from_id, &record->to_id, 1,
                         record->payload, &record->size);
        if ((uint32_t)result != 0x8877001eu)
            break;
        if (record->payload == NULL)
            result = HOST_FN(0x20c, multip_alloc_fn)(1, record->size,
                                                     &record->payload, 0);
        else
            result = HOST_FN(0x210, multip_realloc_fn)(1, record->size,
                                                      &record->payload);
        if (result != 0)
            return -1;
    }
    if (result != 0) {
        if (record->payload != NULL &&
            HOST_FN(0x214, multip_free_fn)(&record->payload) != 0)
            return -1;
        return HOST_FN(0x214, multip_free_fn)((void **)&record) != 0 ? -1 : 0;
    }
    *received = 1;
    record->timestamp = HOST_FN(0x38, multip_ticks_fn)();
    return multip_queue_append((struct multip_queue *)(uintptr_t)
                               M32(M_RECEIVE_QUEUE), record);
}

/* .text+0x5980 -- report whether a receive record was dequeued. */
int32_t FS98_STDCALL multip_receive_take(void **record, uint32_t *received)
{
    *received = 0;
    if (multip_queue_take((struct multip_queue *)(uintptr_t)
                         M32(M_RECEIVE_QUEUE), record) != 0)
        return -1;
    if (*record != NULL)
        *received = 1;
    return 0;
}

/* .text+0x4ce0 -- construct a queue; failure has already cleared *queue. */
int32_t FS98_STDCALL multip_queue_create(struct multip_queue **queue)
{
    struct multip_queue *created;

    *queue = NULL;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, sizeof(*created),
                                       (void **)&created, 1) != 0)
        return -1;
    created->count = 0;
    InitializeCriticalSection(&created->lock);
    created->head = NULL;
    created->tail = NULL;
    *queue = created;
    return 0;
}

/* .text+0x4ed0 -- free records and nodes, but not record-owned payloads.
 * Retail advances head before either free and retains the lock on failure. */
int32_t FS98_STDCALL multip_queue_clear(struct multip_queue *queue)
{
    if (queue == NULL)
        return -1;
    EnterCriticalSection(&queue->lock);
    while (queue->head != NULL) {
        queue->tail = queue->head;
        queue->head = queue->head->next;
        if (HOST_FN(0x214, multip_free_fn)((void **)queue->tail) != 0)
            return -1;
        if (HOST_FN(0x214, multip_free_fn)((void **)&queue->tail) != 0)
            return -1;
    }
    queue->tail = NULL;
    queue->count = 0;
    LeaveCriticalSection(&queue->lock);
    return 0;
}

/* .text+0x4d50 -- do not close events if queue destruction failed. */
int32_t FS98_STDCALL multip_queue_destroy(struct multip_queue **queue)
{
    if (*queue == NULL || multip_queue_clear(*queue) != 0)
        return -1;
    DeleteCriticalSection(&(*queue)->lock);
    return HOST_FN(0x214, multip_free_fn)((void **)queue) != 0 ? -1 : 0;
}

/* .text+0x57a0 -- a receive error exits without clearing the running flag. */
DWORD WINAPI multip_receive_thread(void *parameter)
{
    DWORD wait_result;
    uint32_t received;

    (void)parameter;
    while (M32(M_RECEIVE_RUNNING) != 0) {
        wait_result = WaitForMultipleObjects(2,
            (const HANDLE *)(uintptr_t)(D + M_RECEIVE_STOP), FALSE, INFINITE);
        if (wait_result == WAIT_OBJECT_0)
            M32(M_RECEIVE_RUNNING) = 0;
        else if (wait_result == WAIT_OBJECT_0 + 1) {
            do {
                received = 1;
                if (multip_receive(&received) != 0)
                    return 0xffffffffu;
            } while (received != 0);
        }
    }
    return 0;
}

/* .text+0x7840 -- send-drain failures are ignored by this worker. */
DWORD WINAPI multip_send_thread(void *parameter)
{
    DWORD wait_result;

    (void)parameter;
    while (M32(M_SEND_RUNNING) != 0) {
        wait_result = WaitForMultipleObjects(2,
            (const HANDLE *)(uintptr_t)(D + M_SEND_STOP), FALSE, INFINITE);
        if (wait_result == WAIT_OBJECT_0)
            M32(M_SEND_RUNNING) = 0;
        else if (wait_result == WAIT_OBJECT_0 + 1)
            multip_send_drain();
    }
    return 0;
}

/* .text+0x56a0 and +0x7700 -- work event first, then stop event. */
int32_t multip_receive_setup(void)
{
    M32(M_RECEIVE_EVENT) = ADDR(CreateEventA(NULL, FALSE, FALSE, NULL));
    M32(M_RECEIVE_STOP) = ADDR(CreateEventA(NULL, FALSE, FALSE, NULL));
    return multip_queue_create((struct multip_queue **)(uintptr_t)
                               (D + M_RECEIVE_QUEUE)) != 0 ? -1 : 0;
}

int32_t multip_send_setup(void)
{
    M32(M_SEND_EVENT) = ADDR(CreateEventA(NULL, FALSE, FALSE, NULL));
    M32(M_SEND_STOP) = ADDR(CreateEventA(NULL, FALSE, FALSE, NULL));
    if (multip_queue_create((struct multip_queue **)(uintptr_t)
                           (D + M_SEND_QUEUE_FLAGS)) != 0)
        return -1;
    return multip_queue_create((struct multip_queue **)(uintptr_t)
                               (D + M_SEND_QUEUE_PLAIN)) != 0 ? -1 : 0;
}

/* .text+0x56e0 and +0x7750 -- reverse queue setup order, then work/stop. */
int32_t multip_receive_teardown(void)
{
    if (multip_queue_destroy((struct multip_queue **)(uintptr_t)
                            (D + M_RECEIVE_QUEUE)) != 0)
        return -1;
    if (!CloseHandle((HANDLE)(uintptr_t)M32(M_RECEIVE_EVENT)))
        return -1;
    return CloseHandle((HANDLE)(uintptr_t)M32(M_RECEIVE_STOP)) ? 0 : -1;
}

int32_t multip_send_teardown(void)
{
    if (multip_queue_destroy((struct multip_queue **)(uintptr_t)
                            (D + M_SEND_QUEUE_PLAIN)) != 0)
        return -1;
    if (multip_queue_destroy((struct multip_queue **)(uintptr_t)
                            (D + M_SEND_QUEUE_FLAGS)) != 0)
        return -1;
    if (!CloseHandle((HANDLE)(uintptr_t)M32(M_SEND_EVENT)))
        return -1;
    return CloseHandle((HANDLE)(uintptr_t)M32(M_SEND_STOP)) ? 0 : -1;
}

/* .text+0x5730 and +0x77b0 -- set running before calling CreateThread. */
int32_t multip_receive_start(void)
{
    if (M32(M_RECEIVE_RUNNING) == 0) {
        M32(M_RECEIVE_RUNNING) = 1;
        M32(M_RECEIVE_THREAD) = ADDR(CreateThread(
            (LPSECURITY_ATTRIBUTES)(uintptr_t)(D + 0x538), 0,
            multip_receive_thread, NULL, 0,
            (LPDWORD)(uintptr_t)(D + M_RECEIVE_THREAD_ID)));
    }
    return 0;
}

int32_t multip_send_start(void)
{
    if (M32(M_SEND_RUNNING) == 0) {
        M32(M_SEND_RUNNING) = 1;
        M32(M_SEND_THREAD) = ADDR(CreateThread(
            (LPSECURITY_ATTRIBUTES)(uintptr_t)(D + 0x548), 0,
            multip_send_thread, NULL, 0,
            (LPDWORD)(uintptr_t)(D + M_SEND_THREAD_ID)));
    }
    return 0;
}

/* .text+0x5770 -- the wait result is ignored and the thread handle retained. */
int32_t multip_receive_stop(void)
{
    if (!SetEvent((HANDLE)(uintptr_t)M32(M_RECEIVE_STOP)))
        return -1;
    WaitForSingleObject((HANDLE)(uintptr_t)M32(M_RECEIVE_THREAD), 100);
    return 0;
}

/* .text+0x77f0 -- clear both queues before signalling the stop event. */
int32_t multip_send_stop(void)
{
    if (multip_queue_clear((struct multip_queue *)(uintptr_t)
                          M32(M_SEND_QUEUE_FLAGS)) != 0)
        return -1;
    if (multip_queue_clear((struct multip_queue *)(uintptr_t)
                          M32(M_SEND_QUEUE_PLAIN)) != 0)
        return -1;
    if (!SetEvent((HANDLE)(uintptr_t)M32(M_SEND_STOP)))
        return -1;
    WaitForSingleObject((HANDLE)(uintptr_t)M32(M_SEND_THREAD), 100);
    return CloseHandle((HANDLE)(uintptr_t)M32(M_SEND_THREAD)) ? 0 : -1;
}

/* .text+0x9e00/+0x9e20 -- compose setup/teardown without rolling back failures. */
int32_t multip_workers_setup(void)
{
    if (multip_receive_setup() != 0)
        return -1;
    return multip_send_setup() != 0 ? -1 : 0;
}

int32_t multip_workers_teardown(void)
{
    if (multip_send_teardown() != 0)
        return -1;
    return multip_receive_teardown() != 0 ? -1 : 0;
}

/* .text+0x9e40/+0x9e70 -- shared session flag changes only after both calls. */
int32_t multip_workers_start(void)
{
    if (M32(M_WORKERS_STARTED) == 0) {
        if (multip_send_start() != 0)
            return -1;
        if (multip_receive_start() != 0)
            return -1;
        M32(M_WORKERS_STARTED) = 1;
    }
    return 0;
}

int32_t multip_workers_stop(void)
{
    if (M32(M_WORKERS_STARTED) != 0) {
        if (multip_receive_stop() != 0)
            return -1;
        if (multip_send_stop() != 0)
            return -1;
        M32(M_WORKERS_STARTED) = 0;
    }
    return 0;
}

typedef int32_t (FS98_STDCALL *multip_dp_loader_fn)(void *);
typedef int32_t (FS98_STDCALL *multip_dp_create_fn)(const GUID *, void **, void *);
typedef int32_t (FS98_STDCALL *multip_dp_query_fn)(void *, const GUID *, void **);
typedef uint32_t (FS98_STDCALL *multip_dp_release_fn)(void *);

static const GUID s_iid_directplay2a = {
    0x9d460580, 0xa822, 0x11cf, {0x96, 0x0c, 0x00, 0x80, 0xc7, 0x53, 0x4e, 0x82}
};
static const GUID s_iid_directplay3a = {
    0x133efe41, 0x32dc, 0x11d0, {0x9c, 0xfb, 0x00, 0xa0, 0xc9, 0x0a, 0x43, 0xcb}
};

/* .text+0x9cf0 -- the enumerator export, not the factory, sets status bit 0.
 * Neither a failed load nor an absent enumerator clears a previously set bit. */
int32_t multip_directplay_load(void)
{
    if (HOST_FN(0x124, multip_dp_loader_fn)((void *)(uintptr_t)
                                          (D + M_DIRECTPLAY_EXPORTS)) != 0)
        return -1;
    if (M32(M_DIRECTPLAY_ENUMERATE) != 0)
        M32(M_DIRECTPLAY_LOADED) = 1;
    return 0;
}

/* .text+0x9d20 -- release the live interface before nulling its cell. The
 * status/query flags are retained, even when unloading succeeds. */
int32_t multip_directplay_unload(void)
{
    void *directplay = (void *)(uintptr_t)M32(M_DIRECTPLAY);
    multip_dp_release_fn release;

    if (directplay != NULL) {
        release = *(multip_dp_release_fn *)(void *)
            (*(uint8_t **)directplay + 0x08);
        release(directplay);
    }
    M32(M_DIRECTPLAY) = 0;
    return HOST_FN(0x128, multip_dp_loader_fn)((void *)(uintptr_t)
                                             (D + M_DIRECTPLAY_EXPORTS)) != 0 ? -1 : 0;
}

/* .text+0x9d50 -- query both IIDs on the factory object, not on the first
 * queried interface. A failed first query leaks the factory reference. The
 * second query result affects only the flag; temporary releases still run. */
int32_t FS98_STDCALL multip_directplay_create(const GUID *provider)
{
    void *created;
    void *queried;
    multip_dp_query_fn query;
    multip_dp_release_fn release;

    M32(M_DIRECTPLAY_QUERIED) = 0;
    if ((*(multip_dp_create_fn *)(uintptr_t)(D + M_DIRECTPLAY_CREATE))
        (provider, &created, NULL) != 0)
        return -1;
    query = *(multip_dp_query_fn *)(void *)*(uint8_t **)created;
    if (query(created, &s_iid_directplay2a, &queried) != 0)
        return -1;
    query = *(multip_dp_query_fn *)(void *)*(uint8_t **)created;
    if (query(created, &s_iid_directplay3a,
              (void **)(uintptr_t)(D + M_DIRECTPLAY)) == 0)
        M32(M_DIRECTPLAY_QUERIED) = 1;
    release = *(multip_dp_release_fn *)(void *)(*(uint8_t **)queried + 0x08);
    release(queried);
    release = *(multip_dp_release_fn *)(void *)(*(uint8_t **)created + 0x08);
    release(created);
    return 0;
}

#include "multip_profile.inc"
#include "multip_session.inc"
#include "multip_aircraft.inc"
#include "multip_records.inc"
static uint8_t *FS98_STDCALL multip_find_player(uint32_t player_id);
#include "multip_tracking.inc"
static uint32_t FS98_STDCALL multip_send(uint32_t to_id, const void *payload,
                                        uint32_t size, uint32_t flags);
#include "multip_messages.inc"
#include "multip_players.inc"
#include "multip_callbacks.inc"
#include "multip_motion.inc"
#include "multip_commands.inc"
#include "multip_weather.inc"
#include "multip_tick.inc"
#include "multip_system.inc"
#include "multip_app_players.inc"
#include "multip_updates.inc"
#include "multip_textures.inc"
#include "multip_texture_receive.inc"
#include "multip_transfer_messages.inc"
#include "multip_texture_send.inc"
#include "multip_model_send.inc"
#include "multip_model_cache.inc"
#include "multip_model_receive.inc"
#include "multip_title_receive.inc"
#include "multip_state_messages.inc"
#include "multip_control_updates.inc"
#include "multip_lifecycle.inc"
#include "multip_dispatch.inc"

/* Private entry access for the differential DLL only. Normal builds retain
 * the retail export surface; session and worker code use internal calls. */
#ifdef MULTIP_DIFFTEST
__declspec(dllexport) void *const multip_test_entries[] = {
    (void *)multip_send_one, (void *)multip_send_drain,
    (void *)multip_send_drain_all, (void *)multip_receive,
    (void *)multip_receive_take,
    (void *)multip_receive_setup, (void *)multip_receive_teardown,
    (void *)multip_receive_start, (void *)multip_receive_stop,
    (void *)multip_receive_thread,
    (void *)multip_send_setup, (void *)multip_send_teardown,
    (void *)multip_send_start, (void *)multip_send_stop,
    (void *)multip_send_thread,
    (void *)multip_queue_create, (void *)multip_queue_destroy,
    (void *)multip_queue_clear,
    (void *)multip_workers_setup, (void *)multip_workers_teardown,
    (void *)multip_workers_start, (void *)multip_workers_stop,
    (void *)multip_directplay_load, (void *)multip_directplay_unload,
    (void *)multip_directplay_create,
    (void *)multip_profile_load, (void *)multip_profile_save,
    (void *)multip_session_open_join, (void *)multip_session_enum_groups,
    (void *)multip_session_join, (void *)multip_session_create_groups,
    (void *)multip_session_host, (void *)multip_session_set_player,
    (void *)multip_session_set_observer,
    (void *)multip_session_get_description, (void *)multip_session_set_capacity,
    (void *)multip_session_get_capacity, (void *)multip_player_decode_name,
    (void *)multip_player_encode_name, (void *)multip_player_publish_name,
    (void *)multip_player_reset_collision, (void *)multip_player_count_types,
    (void *)multip_aircraft_select, (void *)multip_aircraft_release,
    (void *)multip_aircraft_copy, (void *)multip_session_record_init,
    (void *)multip_player_name_record_init, (void *)multip_player_name_record_copy,
    (void *)multip_player_name_record_free, (void *)multip_player_names_clear,
    (void *)multip_session_enumerate_players, (void *)multip_track_clear,
    (void *)multip_selection_reset, (void *)multip_track_show,
    (void *)multip_track_callback, (void *)multip_selection_get,
    (void *)multip_selection_view_blocked, (void *)multip_player_refresh_local,
    (void *)multip_selection_apply,
    (void *)multip_message_1001, (void *)multip_message_1008,
    (void *)multip_message_1002, (void *)multip_message_1009,
    (void *)multip_message_1003, (void *)multip_message_1004,
    (void *)multip_message_1005, (void *)multip_message_1006,
    (void *)multip_message_100f, (void *)multip_message_1010,
    (void *)multip_message_1011, (void *)multip_message_100a,
    (void *)multip_message_100c, (void *)multip_message_100e,
    (void *)multip_player_append, (void *)multip_player_mark_unknown,
    (void *)multip_player_add_remote, (void *)multip_player_remove_local,
    (void *)multip_player_remove, (void *)multip_player_remove_all,
    (void *)multip_player_create_local, (void *)multip_player_start,
    (void *)multip_player_enumerate, (void *)multip_session_apply_receive_option,
    (void *)multip_message_1007, (void *)multip_message_1012,
    (void *)multip_session_frame_callback, (void *)multip_session_frame_start,
    (void *)multip_session_frame_stop,
    (void *)multip_players_draw, (void *)multip_players_draw_callback,
    (void *)multip_players_draw_start, (void *)multip_players_draw_stop,
    (void *)multip_player_predict, (void *)multip_players_predict,
    (void *)multip_selection_frame_callback, (void *)multip_selection_frame_start,
    (void *)multip_selection_frame_stop,
    (void *)multip_selection_toggle_mode, (void *)multip_selection_cycle,
    (void *)multip_player_become_observer, (void *)multip_selection_follow,
    (void *)multip_command_callback, (void *)multip_commands_start,
    (void *)multip_commands_stop,
    (void *)multip_message_1013, (void *)multip_message_1014, (void *)multip_message_1017,
    (void *)multip_interval_callback, (void *)multip_sync_callback,
    (void *)multip_maintenance_callback, (void *)multip_guidance_callback,
    (void *)multip_guidance_adf, (void *)multip_guidance_autopilot,
    (void *)multip_tick_start, (void *)multip_tick_stop,
    (void *)multip_system_group, (void *)multip_system_created,
    (void *)multip_system_destroyed, (void *)multip_system_player_data,
    (void *)multip_system_host,
    (void *)multip_handle_1014, (void *)multip_handle_1013,
    (void *)multip_handle_1001, (void *)multip_handle_1002,
    (void *)multip_handle_1008, (void *)multip_handle_1009,
    (void *)multip_handle_1003, (void *)multip_handle_1004,
    (void *)multip_handle_1007, (void *)multip_handle_100a,
    (void *)multip_handle_100c, (void *)multip_handle_100d,
    (void *)multip_handle_100f, (void *)multip_handle_1011,
    (void *)multip_handle_1012, (void *)multip_message_100d,
    (void *)multip_handle_1000, (void *)multip_player_motion_init,
    (void *)multip_handle_1018, (void *)multip_handle_1019, (void *)multip_handle_1017,
    (void *)multip_trim_spaces, (void *)multip_filename_extension, (void *)multip_texture_path,
    (void *)multip_texture_list_contains, (void *)multip_texture_exists, (void *)multip_texture_catalogue,
    (void *)multip_message_1015, (void *)multip_textures_request, (void *)multip_texture_read,
    (void *)multip_texture_write, (void *)multip_textures_cleanup, (void *)multip_message_1016,
    (void *)multip_texture_receive_owner, (void *)multip_texture_receive_file,
    (void *)multip_texture_receive_ensure, (void *)multip_texture_receive_add,
    (void *)multip_texture_receive_remove, (void *)multip_texture_receive_remove_owner,
    (void *)multip_texture_receive_clear, (void *)multip_handle_101d,
    (void *)multip_handle_101e, (void *)multip_handle_101f,
    (void *)multip_message_101a, (void *)multip_message_101b, (void *)multip_message_101c,
    (void *)multip_message_101d, (void *)multip_message_101e,
    (void *)multip_texture_send_find, (void *)multip_texture_send_cache,
    (void *)multip_texture_send_uncache, (void *)multip_texture_send_add, (void *)multip_texture_send_remove,
    (void *)multip_texture_send_stop, (void *)multip_texture_send_start, (void *)multip_texture_send,
    (void *)multip_texture_send_callback, (void *)multip_handle_1015,
    (void *)multip_model_send_stop, (void *)multip_model_send_add, (void *)multip_model_send,
    (void *)multip_model_send_clear, (void *)multip_model_send_cancel, (void *)multip_model_send_remove,
    (void *)multip_model_send_callback, (void *)multip_handle_1010,
    (void *)multip_model_cache_prepare, (void *)multip_model_cache_write, (void *)multip_model_cache_read,
    (void *)multip_model_cache_select, (void *)multip_model_receive_find, (void *)multip_model_receive_remove,
    (void *)multip_handle_101a, (void *)multip_handle_101b, (void *)multip_handle_101c,
    (void *)multip_model_cache_exists, (void *)multip_handle_100e,
    (void *)multip_motion_is_blocked, (void *)multip_message_1000,
    (void *)multip_message_1018, (void *)multip_message_1019,
    (void *)multip_control_changes, (void *)multip_control_collisions, (void *)multip_state_publish,
    (void *)multip_control_publish, (void *)multip_control_callback,
    (void *)multip_receive_callbacks_start, (void *)multip_receive_callbacks_stop,
    (void *)multip_callbacks_start, (void *)multip_callbacks_stop,
    (void *)multip_session_disconnect, (void *)multip_session_complete,
    (void *)multip_handle_1005, (void *)multip_handle_1006, (void *)multip_handle_1016,
    (void *)multip_system_lost, (void *)multip_system_dispatch,
    (void *)multip_app_dispatch, (void *)multip_receive_dispatch,
    (void *)multip_receive_expire, (void *)multip_receive_callback
};
#endif

/* .text+0x7890, +0x87e0 and public +0x20 (.text+0x9840).
 * Allocation/copy failures leak partially built records in retail. There is
 * no connected-state guard, and queue selection tests the entire flags word. */
static uint32_t FS98_STDCALL multip_send(uint32_t to_id, const void *payload,
                                        uint32_t size, uint32_t flags)
{
    struct multip_send_record *record;
    struct multip_queue *queue;

    if (M32(M_GUARANTEED_MESSAGES) == 1)
        flags |= 1;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, sizeof(*record),
                                       (void **)&record, 1) != 0)
        return 0xffffffffu;
    record->to_id = to_id;
    record->size = size;
    record->flags = flags;
    if (HOST_FN(0x20c, multip_alloc_fn)(1, size, &record->payload, 0) != 0)
        return 0xffffffffu;
    if (HOST_FN(0x21c, multip_copy_fn)(record->payload, payload, size) != 0)
        return 0xffffffffu;
    queue = (struct multip_queue *)(uintptr_t)
        M32(flags != 0 ? M_SEND_QUEUE_FLAGS : M_SEND_QUEUE_PLAIN);
    if (multip_queue_append(queue, record) != 0)
        return 0xffffffffu;
    return SetEvent((HANDLE)(uintptr_t)M32(M_SEND_EVENT)) ? 0 : 0xffffffffu;
}

/* .text+0x8620 -- local player first, then remote records in reverse order. */
static uint8_t *FS98_STDCALL multip_find_player(uint32_t player_id)
{
    uint8_t *player = (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
    uint8_t *players;
    uint32_t count;

    if (player != NULL &&
        *(uint32_t *)(void *)(player + 0x04) == player_id)
        return player;

    count = M32(M_PLAYER_COUNT);
    players = (uint8_t *)(uintptr_t)M32(M_PLAYERS);
    while (count != 0) {
        player = players + (--count * MULTIP_PLAYER_SIZE);
        if (*(uint32_t *)(void *)(player + 0x04) == player_id)
            return player;
    }
    return NULL;
}

/* +0x24, .text+0x9de0 -- the list consumed by DYNAMIC and MPCHAT. */
static uint32_t *multip_get_players(void)
{
    M32(M_PLAYERS_RESULT + 0x00) = M32(M_PLAYER_COUNT);
    M32(M_PLAYERS_RESULT + 0x04) = M32(M_PLAYERS);
    return (uint32_t *)(uintptr_t)(D + M_PLAYERS_RESULT);
}

/* +0x28, .text+0x8680. */
static uint8_t *FS98_STDCALL multip_get_player(uint32_t player_id)
{
    return multip_find_player(player_id);
}

/* +0x2c, .text+0x8690. */
static uint8_t *multip_get_local_player(void)
{
    return (uint8_t *)(uintptr_t)M32(M_LOCAL_PLAYER);
}

/* .text+0xa850 -- true when a connected type-1 player has its +0x0c clear. */
static uint32_t multip_has_unready_player(void)
{
    uint8_t *players;
    uint8_t *player;
    uint32_t count;

    if (M32(M_CONNECTED) == 0)
        return 0;

    count = M32(M_PLAYER_COUNT);
    players = (uint8_t *)(uintptr_t)M32(M_PLAYERS);
    while (count != 0) {
        player = players + (--count * MULTIP_PLAYER_SIZE);
        if (*(uint32_t *)(void *)(player + 0x08) == 1 &&
            *(uint32_t *)(void *)(player + 0x0c) == 0)
            return 1;
    }
    return 0;
}

/* +0x30, .text+0xa560 -- DirectPlay present, connected, unready player. */
static uint32_t multip_get_status(void)
{
    uint32_t status = 0;

    if (M32(M_DIRECTPLAY_LOADED) != 0)
        status |= 1;
    if (M32(M_CONNECTED) != 0)
        status |= 2;
    if (multip_has_unready_player() != 0)
        status |= 4;
    return status;
}

/* +0x34, .text+0xa5b0 -- MAIN's collision result override. */
static uint32_t FS98_STDCALL multip_override_collision(int32_t *result)
{
    uint8_t *player;

    if (M32(M_CONNECTED) != 0 && M32(M_AIRCRAFT_ACTIVE) != 0 &&
        M32(M_PLAYER_MODE) == 1) {
        player = multip_find_player(M32(M_ACTIVE_PLAYER_ID));
        if (player != NULL &&
            *(uint32_t *)(void *)(player + 0x28) != 0)
            *result = -1;
    }
    return 0;
}

typedef void (FS98_STDCALL *util_range48_fn)(const void *, const void *,
                                              int32_t, int32_t *, int16_t *,
                                              uint8_t *);

/* MSVCRT _ftol truncates the x87 value to a signed 64-bit integer. */
static int32_t multip_distance_to_tenths(uint32_t distance)
{
    static const double s_distance_factor = 0.0005396115152459395;
    static const double s_ten = 10.0;
    uint64_t input = distance;
    int64_t output;
    uint16_t control_word;
    uint16_t truncate_word;

    __asm__ volatile("fnstcw %0" : "=m"(control_word));
    truncate_word = control_word | 0x0c00;
    __asm__ volatile("fildll %1\n\t"
                     "fmull %2\n\t"
                     "fmull %3\n\t"
                     "fldcw %4\n\t"
                     "fistpll %0\n\t"
                     "fldcw %5"
                     : "=m"(output)
                     : "m"(input), "m"(s_distance_factor), "m"(s_ten),
                       "m"(truncate_word), "m"(control_word)
                     : "st", "memory");
    return (int32_t)output;
}

/* +0x38, .text+0xa5f0 -- range to the active remote aircraft. */
static uint32_t FS98_STDCALL multip_get_active_range(int32_t *result)
{
    uint8_t *player;
    uint8_t *aircraft;
    util_range48_fn range48;
    int32_t distance;
    int16_t bearing;
    uint8_t scale;

    if (M32(M_CONNECTED) == 0 || M32(M_AIRCRAFT_ACTIVE) == 0 ||
        M32(M_PLAYER_MODE) == 0)
        return 0;

    player = multip_find_player(M32(M_ACTIVE_PLAYER_ID));
    if (player == NULL)
        return 0;
    aircraft = (uint8_t *)(uintptr_t)*(uint32_t *)(void *)(player + 0x28);
    if (aircraft == NULL)
        return 0;

    range48 = *(util_range48_fn *)(uintptr_t)
        (M32(IMPORT_UTIL) + 0x64);
    range48((const void *)(uintptr_t)(M32(IMPORT_GLOBAL) + 0x560),
            aircraft + 0x04, 3, &distance, &bearing, &scale);
    *result = multip_distance_to_tenths((uint32_t)distance);
    return 0;
}

static void multip_install(void)
{
    static const uint32_t s_imports[] = {
        FS98_MOD_HOST, FS98_MOD_GLOBAL, FS98_MOD_ACONTAIN, FS98_MOD_CHAIN,
        FS98_MOD_HG2D, FS98_MOD_G3D, FS98_MOD_MAIN, FS98_MOD_NUMERIC,
        FS98_MOD_UTIL, FS98_MOD_WEATHERM
    };
    uint32_t index;
    const struct multip_data_pointer *pointer;
    uint8_t *owner;
    uint32_t target;

    for (index = 0; index < sizeof(s_multip_data_pointers) /
                                 sizeof(s_multip_data_pointers[0]); index++) {
        pointer = &s_multip_data_pointers[index];
        if (pointer->kind == 1)
            continue; /* Each recovered callback is installed by its owner. */
        owner = pointer->owner == 0 ? s_multip_image.data : s_multip_rdata;
        target = pointer->kind == 2 ? D + pointer->target :
            ADDR(s_multip_rdata + pointer->target);
        *(uint32_t *)(void *)(owner + pointer->offset) = target;
    }

    for (index = 0; index < sizeof(s_imports) / sizeof(s_imports[0]); index++)
        M32(index * 8) = s_imports[index];

    M32(0x538) = 12;
    M32(0x548) = 12;
    M32(M_DIRECTPLAY_EXPORTS + 0x00) = D + M_DIRECTPLAY_CREATE;
    M32(M_DIRECTPLAY_EXPORTS + 0x04) = D + M_DIRECTPLAY_ENUMERATE;
    M32(M_DIRECTPLAY_EXPORTS + 0x08) = D + M_DIRECTPLAY_LOBBY;

    M32(MULTIP_LINKAGE + 0x00) = FS98_MOD_MULTIP;
    M32(MULTIP_LINKAGE + 0x04) = ADDR(multip_unimplemented);
    M32(MULTIP_LINKAGE + 0x08) = ADDR(multip_unimplemented);
    M32(MULTIP_LINKAGE + 0x18) = ADDR(multip_unimplemented);
    M32(MULTIP_LINKAGE + 0x1c) = ADDR(multip_unimplemented);
    M32(MULTIP_LINKAGE + 0x20) = ADDR(multip_send);
    M32(MULTIP_LINKAGE + 0x24) = ADDR(multip_get_players);
    M32(MULTIP_LINKAGE + 0x28) = ADDR(multip_get_player);
    M32(MULTIP_LINKAGE + 0x2c) = ADDR(multip_get_local_player);
    M32(MULTIP_LINKAGE + 0x30) = ADDR(multip_get_status);
    M32(MULTIP_LINKAGE + 0x34) = ADDR(multip_override_collision);
    M32(MULTIP_LINKAGE + 0x38) = ADDR(multip_get_active_range);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        multip_install();
    }
    return TRUE;
}
