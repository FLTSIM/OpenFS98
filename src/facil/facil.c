/* FACILITIES.DLL -- module id 37: the compiled facility index.
 *
 * FE exposes the facility-directory blocks embedded in scenery files.  FACIL
 * turns those blocks into INDEX_FILE, a small hierarchy used by the airport
 * and scenery dialogs.  This file currently covers the non-UI half: profile
 * state, index validation, hierarchy construction and serialization.  The
 * three dialog services remain deliberately inert until their ofstest pass.
 *
 * The complete retail .data image is retained.  Besides making the recovered
 * offsets useful documentation, this preserves the dialog tables while the UI
 * half is still being transliterated.  Like retail, this is a 32-bit Win32 ABI
 * module and the image contains live pointers.
 */
#include <windows.h>
#include <stdint.h>

#include "fs98/linkage.h"
#include "fs98/host.h"

#define FACIL_IMAGE_SIZE 0x2d1c
#define FACIL_DATA_START 0x0074
#define FACIL_NSLOTS     7

typedef uint32_t __attribute__((may_alias)) facil_u32;

typedef void (*facil_fn)(void);

struct facil_linkage {
    uint32_t id;
    uint32_t (FS98_STDCALL *init)(void);
    uint32_t (FS98_STDCALL *deinit)(void);
    uint32_t reserved_0c;
    uint32_t reserved_10;
    void *dialogs;
    facil_fn slots[FACIL_NSLOTS];
};

struct facil_image {
    struct fs98_import import[8];
    struct facil_linkage linkage;
    uint8_t data[FACIL_IMAGE_SIZE - FACIL_DATA_START];
};

extern struct facil_image facil_image;

typedef char (*facil_layout_check)[
    sizeof(struct facil_image) == FACIL_IMAGE_SIZE &&
    sizeof(struct facil_linkage) == 0x34 ? 1 : -1];

#define D                    ((uint8_t *)&facil_image)
#define D32(offset)          (*(facil_u32 *)(D + (offset)))
#define DPTR(offset, type)   (*(type **)(D + (offset)))
#define IMPORT(index)        (facil_image.import[(index)].link)
#define HOST                 IMPORT(0)
#define FE                   IMPORT(3)
#define UTIL                 IMPORT(6)
#define SLOT(module, offset, type) \
    (*(type *)((uint8_t *)(module) + (offset)))

struct facil_source {
    uint8_t id[16];
    uint32_t value;
};

struct facil_tree {
    struct fs98_host_list order;
    struct fs98_host_list nodes;
};

struct facil_node {
    const char *name;
    uint32_t kind;
    struct fs98_host_list sources;
    uint32_t children_ready;
    struct facil_tree children;
};

struct facil_leaf {
    const char *name;
    uint32_t kind;
    struct fs98_host_list sources;
};

struct facil_disk_node {
    uint32_t parent;
    uint32_t sibling;
    uint32_t child;
    uint32_t kind;
    uint32_t payload_size;
    uint32_t flags;
    uint32_t name_size;
    uint32_t source_count;
    char name_and_sources[0x1000];
};

typedef char (*facil_tree_check)[
    sizeof(struct facil_source) == 0x14 &&
    sizeof(struct facil_tree) == 0x38 &&
    sizeof(struct facil_node) == 0x60 &&
    sizeof(struct facil_leaf) == 0x24 ? 1 : -1];

struct fs98_host_file;

typedef int32_t (FS98_STDCALL *facil_cfg_int_fn)(const char *, const char *,
                                                 int32_t *);
typedef int32_t (FS98_STDCALL *facil_cfg_put_int_fn)(const char *, const char *,
                                                     int32_t);
typedef int32_t (FS98_STDCALL *facil_cfg_put_string_fn)(const char *,
                                                        const char *,
                                                        const char *);
typedef int32_t (FS98_STDCALL *facil_get_attributes_fn)(int32_t *, const char *,
                                                        int32_t);
typedef int32_t (FS98_STDCALL *facil_set_attributes_fn)(int32_t, const char *,
                                                        int32_t);
typedef int32_t (FS98_STDCALL *facil_file_create_fn)(struct fs98_host_file **,
                                                     const char *, int32_t);
typedef int32_t (FS98_STDCALL *facil_file_delete_fn)(const char *, int32_t);
typedef int32_t (FS98_STDCALL *facil_file_move_fn)(const char *, int32_t,
                                                   const char *, int32_t);
typedef int32_t (FS98_STDCALL *facil_temp_fn)(char *, int32_t);
typedef int32_t (FS98_STDCALL *facil_file_tell_fn)(struct fs98_host_file *,
                                                   uint32_t *);
typedef int32_t (FS98_STDCALL *facil_file_size_fn)(struct fs98_host_file *,
                                                   uint32_t *);
typedef int32_t (FS98_STDCALL *facil_file_write_fn)(struct fs98_host_file *,
                                                    const void *, uint32_t,
                                                    uint32_t *);
typedef int32_t (FS98_STDCALL *facil_loadstring_alloc_fn)(uint32_t, char **);

typedef uint32_t (FS98_STDCALL *facil_fe_list_fn)(uint8_t **, uint32_t *);
typedef uint32_t (FS98_STDCALL *facil_fe_open_fn)(const uint8_t *, uint8_t **);
typedef uint32_t (FS98_STDCALL *facil_fe_read_fn)(const struct facil_source *,
                                                  uint32_t, uint32_t,
                                                  uint8_t **, uint32_t *);

typedef int32_t (FS98_STDCALL *facil_alert_fn)(void **, uint32_t, uint32_t,
                                               uint32_t, uint32_t, uint32_t,
                                               uint32_t, uint32_t);
typedef int32_t (FS98_STDCALL *facil_alert_close_fn)(void **);

#define HOST_CFG_INT          SLOT(HOST, 0x0a8, facil_cfg_int_fn)
#define HOST_CFG_PUT_INT      SLOT(HOST, 0x0ac, facil_cfg_put_int_fn)
#define HOST_CFG_PUT_STRING   SLOT(HOST, 0x0cc, facil_cfg_put_string_fn)
#define HOST_GET_ATTRIBUTES   SLOT(HOST, 0x134, facil_get_attributes_fn)
#define HOST_SET_ATTRIBUTES   SLOT(HOST, 0x138, facil_set_attributes_fn)
#define HOST_FILE_CREATE      SLOT(HOST, 0x148, facil_file_create_fn)
#define HOST_FILE_DELETE      SLOT(HOST, 0x14c, facil_file_delete_fn)
#define HOST_FILE_MOVE        SLOT(HOST, 0x16c, facil_file_move_fn)
#define HOST_TEMP             SLOT(HOST, 0x174, facil_temp_fn)
#define HOST_FILE_TELL        SLOT(HOST, 0x190, facil_file_tell_fn)
#define HOST_FILE_SIZE        SLOT(HOST, 0x1a4, facil_file_size_fn)
#define HOST_FILE_WRITE       SLOT(HOST, 0x1b0, facil_file_write_fn)
#define HOST_LOADSTRING_ALLOC SLOT(HOST, 0x2a4, facil_loadstring_alloc_fn)

#define FE_LIST               SLOT(FE, 0x7c, facil_fe_list_fn)
#define FE_OPEN               SLOT(FE, 0x80, facil_fe_open_fn)
#define FE_READ               SLOT(FE, 0x84, facil_fe_read_fn)
#define UTIL_ALERT            SLOT(UTIL, 0xa8, facil_alert_fn)
#define UTIL_ALERT_CLOSE      SLOT(UTIL, 0xac, facil_alert_close_fn)

#define ROOT_TREE             ((struct facil_tree *)(D + 0x1af8))
#define LAST_OFFSETS          ((uint32_t *)(D + 0x1ae0))
#define RECORD_BUFFER         ((struct facil_disk_node *)(D + 0x1b30))
#define ACTIVE_ID             DPTR(0x2b30, uint8_t)
#define ACTIVE_REGION         DPTR(0x2b34, struct facil_node)
#define ACTIVE_COUNTRY        DPTR(0x2b38, struct facil_node)
#define ACTIVE_STATE          DPTR(0x2b3c, struct facil_node)
#define ACTIVE_CITY           DPTR(0x2b40, struct facil_node)
#define ACTIVE_AIRPORT        DPTR(0x2b44, struct facil_leaf)
#define EMPTY_NAME            ((const char *)(D + 0x2b48))

static void facil_tree_init(struct facil_tree *tree, uint32_t node_size)
{
    tree->order.count = 0;
    tree->order.capacity = 0;
    tree->order.grow = 0x20;
    tree->order.elem_size = 4;
    tree->order.iterating = 0;
    tree->order.pending = 0;
    tree->order.data = NULL;
    tree->nodes.count = 0;
    tree->nodes.capacity = 0;
    tree->nodes.grow = 0x20;
    tree->nodes.elem_size = (int32_t)node_size;
    tree->nodes.iterating = 0;
    tree->nodes.pending = 0;
    tree->nodes.data = NULL;
}

static void facil_clear_active(void)
{
    ACTIVE_REGION = NULL;
    ACTIVE_COUNTRY = NULL;
    ACTIVE_STATE = NULL;
    ACTIVE_CITY = NULL;
    ACTIVE_AIRPORT = NULL;
}

static int32_t facil_tree_free(struct facil_tree *tree, uint32_t depth)
{
    uint32_t index;

    for (index = (uint32_t)tree->order.count; index != 0; index--) {
        uint32_t offset = ((uint32_t *)tree->order.data)[index - 1];
        struct facil_node *node =
            (struct facil_node *)((uint8_t *)tree->nodes.data + offset);
        void *source_data;

        if (depth != 0 &&
            facil_tree_free(&node->children, depth - 1) != 0)
            return -1;
        source_data = node->sources.data;
        node->sources.count = 0;
        node->sources.capacity = 0;
        node->sources.grow = 0x20;
        node->sources.elem_size = 0x14;
        node->sources.iterating = 0;
        node->sources.pending = 0;
        node->sources.data = NULL;
        if (FS98_HOST_FREE(HOST)(&source_data) != 0)
            return -1;
    }
    {
        void *nodes = tree->nodes.data;
        void *order = tree->order.data;

        facil_tree_init(tree, depth == 0 ? 0x24 : 0x60);
        if (FS98_HOST_FREE(HOST)(&nodes) != 0)
            return -1;
        if (FS98_HOST_FREE(HOST)(&order) != 0)
            return -1;
    }
    return 0;
}

static int32_t facil_reset_tree(void)
{
    if (facil_tree_free(ROOT_TREE, 4) != 0)
        return -1;
    facil_clear_active();
    return 0;
}

static void facil_source_copy(struct facil_source *destination,
                              const uint8_t *id, uint32_t value)
{
    uint32_t index;

    for (index = 0; index < 16; index++)
        destination->id[index] = id[index];
    destination->value = value;
}

static int facil_source_exists(struct fs98_host_list *sources,
                               const uint8_t *id, uint32_t value)
{
    uint32_t index;

    for (index = (uint32_t)sources->count; index != 0; index--) {
        struct facil_source *source =
            (struct facil_source *)sources->data + index - 1;
        if (source->value == value &&
            FS98_HOST_MEMCMP(HOST)(id, source->id, 16) == 0)
            return 1;
    }
    return 0;
}

static void *facil_tree_add(struct facil_tree *tree, uint32_t node_size,
                            const char *name, uint32_t kind,
                            const uint8_t *id, uint32_t value)
{
    int32_t low = 0;
    int32_t high = tree->order.count - 1;
    int32_t insertion = 0;
    struct facil_leaf *node;

    while (low <= high) {
        int32_t middle = (low + high) / 2;
        uint32_t offset = ((uint32_t *)tree->order.data)[middle];
        struct facil_leaf *candidate =
            (struct facil_leaf *)((uint8_t *)tree->nodes.data + offset);
        int32_t comparison = FS98_HOST_STRCMP(HOST)(name, candidate->name);

        if (comparison == 0) {
            if (kind < candidate->kind)
                comparison = -1;
            else if (kind > candidate->kind)
                comparison = 1;
        }
        if (comparison == 0) {
            if (!facil_source_exists(&candidate->sources, id, value)) {
                struct facil_source *source =
                    (struct facil_source *)FS98_HOST_LIST_ADD(HOST)(
                        &candidate->sources);
                facil_source_copy(source, id, value);
            }
            return candidate;
        }
        if (comparison < 0)
            high = middle - 1;
        else
            low = middle + 1;
    }
    insertion = low;
    node = (struct facil_leaf *)FS98_HOST_LIST_ADD(HOST)(&tree->nodes);
    node->name = name;
    node->kind = kind;
    node->sources.count = 0;
    node->sources.capacity = 0;
    node->sources.grow = 0x20;
    node->sources.elem_size = 0x14;
    node->sources.iterating = 0;
    node->sources.pending = 0;
    node->sources.data = NULL;
    {
        struct facil_source *source =
            (struct facil_source *)FS98_HOST_LIST_ADD(HOST)(&node->sources);
        uint32_t *order;
        int32_t index;

        facil_source_copy(source, id, value);
        order = (uint32_t *)FS98_HOST_LIST_ADD(HOST)(&tree->order);
        (void)order;
        order = (uint32_t *)tree->order.data;
        for (index = tree->order.count - 1; index > insertion; index--)
            order[index] = order[index - 1];
        order[insertion] =
            (uint32_t)(tree->nodes.count - 1) * node_size;
    }
    return node;
}

static void facil_prepare_children(struct facil_node *node, uint32_t size)
{
    if (!node->children_ready) {
        facil_tree_init(&node->children, size);
        node->children_ready = 1;
    }
}

static struct facil_node *facil_add_region(const char *name, uint32_t kind,
                                            uint32_t value)
{
    struct facil_node *node;

    if (ACTIVE_REGION == NULL)
        facil_tree_init(ROOT_TREE, 0x60);
    node = (struct facil_node *)facil_tree_add(ROOT_TREE, 0x60, name, kind,
                                               ACTIVE_ID, value);
    ACTIVE_REGION = node;
    ACTIVE_COUNTRY = NULL;
    ACTIVE_STATE = NULL;
    ACTIVE_CITY = NULL;
    ACTIVE_AIRPORT = NULL;
    return node;
}

static struct facil_node *facil_add_country(const char *name, uint32_t kind,
                                             uint32_t value)
{
    if (ACTIVE_REGION == NULL)
        facil_add_region(EMPTY_NAME, 0x10000, 0);
    facil_prepare_children(ACTIVE_REGION, 0x60);
    ACTIVE_COUNTRY = (struct facil_node *)facil_tree_add(
        &ACTIVE_REGION->children, 0x60, name, kind, ACTIVE_ID, value);
    ACTIVE_STATE = NULL;
    ACTIVE_CITY = NULL;
    ACTIVE_AIRPORT = NULL;
    return ACTIVE_COUNTRY;
}

static struct facil_node *facil_add_state(const char *name, uint32_t kind,
                                           uint32_t value)
{
    if (ACTIVE_COUNTRY == NULL)
        facil_add_country(EMPTY_NAME, 0x20000, 0);
    facil_prepare_children(ACTIVE_COUNTRY, 0x60);
    ACTIVE_STATE = (struct facil_node *)facil_tree_add(
        &ACTIVE_COUNTRY->children, 0x60, name, kind, ACTIVE_ID, value);
    ACTIVE_CITY = NULL;
    ACTIVE_AIRPORT = NULL;
    return ACTIVE_STATE;
}

static struct facil_node *facil_add_city(const char *name, uint32_t kind,
                                          uint32_t value)
{
    if (ACTIVE_STATE == NULL)
        facil_add_state(EMPTY_NAME, 0x30000, 0);
    facil_prepare_children(ACTIVE_STATE, 0x60);
    ACTIVE_CITY = (struct facil_node *)facil_tree_add(
        &ACTIVE_STATE->children, 0x60, name, kind, ACTIVE_ID, value);
    ACTIVE_AIRPORT = NULL;
    return ACTIVE_CITY;
}

static struct facil_leaf *facil_add_airport(const uint8_t *record)
{
    if (ACTIVE_CITY == NULL)
        facil_add_city(EMPTY_NAME, 0x40000, 0);
    facil_prepare_children(ACTIVE_CITY, 0x24);
    ACTIVE_AIRPORT = (struct facil_leaf *)facil_tree_add(
        &ACTIVE_CITY->children, 0x24, (const char *)record + 0x10,
        *(const facil_u32 *)(record + 8), ACTIVE_ID,
        *(const facil_u32 *)(record + 0x0c));
    return ACTIVE_AIRPORT;
}

static int32_t facil_parse_block(const uint8_t *block)
{
    uint32_t count = *(const facil_u32 *)(block + 0x14);
    const uint8_t *record = block + 0x18;

    while (count-- != 0) {
        uint32_t kind = *(const facil_u32 *)(record + 8);
        uint32_t value = *(const facil_u32 *)(record + 0x0c);
        const char *name = (const char *)record + 0x10;

        switch (kind) {
        case 0x10001: facil_add_region(name, kind, value); break;
        case 0x20001: facil_add_country(name, kind, value); break;
        case 0x30001: facil_add_state(name, kind, value); break;
        case 0x40001: facil_add_city(name, kind, value); break;
        case 0x50001:
        case 0x50002:
        case 0x50003:
        case 0x50004:
            facil_add_airport(record);
            break;
        }
        record += *(const facil_u32 *)(record + 4) + 8;
    }
    return 0;
}

static int32_t FS98_STDCALL facil_compare_ids(const void *left,
                                               const void *right)
{
    return FS98_HOST_MEMCMP(HOST)(left, right, 16);
}

static void facil_sort_ids(uint8_t *ids, uint32_t count)
{
    FS98_HOST_QSORT(HOST)(ids, count, 16, facil_compare_ids);
}

static void facil_unique_ids(uint8_t *ids, uint32_t *count)
{
    uint32_t read;
    uint32_t write = 1;

    if (*count <= 1)
        return;
    for (read = 1; read < *count; read++) {
        if (FS98_HOST_MEMCMP(HOST)(ids + (write - 1) * 16,
                                   ids + read * 16, 16) != 0) {
            if (write != read)
                FS98_HOST_MEMMOVE(HOST)(ids + write * 16,
                                        ids + read * 16, 16);
            write++;
        }
    }
    *count = write;
}

static int32_t facil_read_old_ids(uint8_t **ids, uint32_t *count)
{
    char name[0x104];
    struct fs98_host_file *file;
    uint32_t got;
    uint32_t bytes;

    *ids = NULL;
    *count = 0;
    FS98_HOST_LOADSTRING(HOST)(0x9101, name, sizeof(name));
    if (FS98_HOST_EXISTS(HOST)(name, 0) != 0)
        return 0;
    if (FS98_HOST_FILE_OPEN(HOST)(&file, name, 0, 0) != 0)
        return -1;
    if (FS98_HOST_FILE_SEEK(HOST)(file, 8, 0) != 0 ||
        FS98_HOST_FILE_READ(HOST)(file, count, 4, &got) != 0 || got != 4)
        return -1;
    bytes = *count * 16;
    if (FS98_HOST_ALLOC(HOST)(1, bytes, (void **)ids, 0) != 0)
        return -1;
    if (FS98_HOST_FILE_READ(HOST)(file, *ids, bytes, &got) != 0)
        return -1;
    if (FS98_HOST_FILE_CLOSE(HOST)(file) != 0)
        return -1;
    return got == bytes ? 0 : -1;
}

static int facil_index_changed(const uint8_t *old_ids, uint32_t old_count,
                               const uint8_t *new_ids, uint32_t new_count)
{
    char *name = NULL;
    uint32_t index;

    HOST_LOADSTRING_ALLOC(0x9101, &name);
    if (FS98_HOST_EXISTS(HOST)(name, 0) != 0) {
        FS98_HOST_FREE(HOST)((void **)&name);
        return 1;
    }
    FS98_HOST_FREE(HOST)((void **)&name);
    if (old_count != new_count)
        return 1;
    for (index = 0; index < old_count; index++)
        if (FS98_HOST_MEMCMP(HOST)(old_ids + index * 16,
                                   new_ids + index * 16, 16) != 0)
            return 1;
    return 0;
}

static int32_t facil_backfill(struct fs98_host_file *file, uint32_t offset,
                              uint32_t value)
{
    uint32_t old_position;
    uint32_t wrote;

    if (HOST_FILE_TELL(file, &old_position) != 0 ||
        FS98_HOST_FILE_SEEK(HOST)(file, offset, 0) != 0 ||
        HOST_FILE_WRITE(file, &value, 4, &wrote) != 0 || wrote != 4 ||
        FS98_HOST_FILE_SEEK(HOST)(file, old_position, 0) != 0)
        return -1;
    return 0;
}

static int32_t facil_or_field(struct fs98_host_file *file, uint32_t offset,
                              uint32_t bits)
{
    uint32_t old_position;
    uint32_t value;
    uint32_t got;
    uint32_t wrote;

    if (HOST_FILE_TELL(file, &old_position) != 0 ||
        FS98_HOST_FILE_SEEK(HOST)(file, offset, 0) != 0 ||
        FS98_HOST_FILE_READ(HOST)(file, &value, 4, &got) != 0 || got != 4)
        return -1;
    value |= bits;
    if (FS98_HOST_FILE_SEEK(HOST)(file, offset, 0) != 0 ||
        HOST_FILE_WRITE(file, &value, 4, &wrote) != 0 || wrote != 4 ||
        FS98_HOST_FILE_SEEK(HOST)(file, old_position, 0) != 0)
        return -1;
    return 0;
}

static int32_t facil_write_node(struct fs98_host_file *file,
                                struct facil_disk_node *record,
                                uint32_t size, uint32_t level,
                                struct facil_node *node);

static int32_t facil_write_tree(struct fs98_host_file *file,
                                struct facil_tree *tree, uint32_t parent,
                                uint32_t level)
{
    uint32_t index;

    for (index = 0; index < (uint32_t)tree->order.count; index++) {
        uint32_t offset = ((uint32_t *)tree->order.data)[index];
        struct facil_node *node =
            (struct facil_node *)((uint8_t *)tree->nodes.data + offset);
        uint32_t name_size;
        uint32_t source_size;
        uint32_t total_size;

        RECORD_BUFFER->parent = parent;
        RECORD_BUFFER->sibling = 0;
        RECORD_BUFFER->child = 0;
        RECORD_BUFFER->kind = node->kind;
        RECORD_BUFFER->flags = 0;
        name_size = FS98_HOST_STRLEN(HOST)(node->name) + 1;
        RECORD_BUFFER->name_size = name_size;
        RECORD_BUFFER->source_count = (uint32_t)node->sources.count;
        FS98_HOST_STRCPY(HOST)(RECORD_BUFFER->name_and_sources, node->name);
        source_size = (uint32_t)node->sources.count * 0x14;
        FS98_HOST_MEMMOVE(HOST)(
            (uint8_t *)RECORD_BUFFER->name_and_sources + name_size,
            node->sources.data, source_size);
        RECORD_BUFFER->payload_size = name_size + source_size + 0x0c;
        total_size = name_size + source_size + 0x20;
        if (facil_write_node(file, RECORD_BUFFER, total_size, level, node) != 0)
            return -1;
        if (node->kind == 0x50001) {
            uint32_t node_offset = LAST_OFFSETS[level];
            uint8_t *section = NULL;
            uint32_t section_size = 0;
            struct facil_disk_node fallback;
            struct facil_source *source =
                (struct facil_source *)node->sources.data;
            const char *fallback_name = EMPTY_NAME;

            if (facil_or_field(file, node_offset + 0x14, 2) != 0)
                return -1;
            if (!FE_READ(source, source->value, 0x10006,
                         &section, &section_size))
                return -1;
            if (section != NULL)
                fallback_name = (const char *)section + 8;
            fallback.parent = parent;
            fallback.sibling = 0;
            fallback.child = 0;
            fallback.kind = 0x50002;
            fallback.flags = 0;
            fallback.name_size = FS98_HOST_STRLEN(HOST)(fallback_name) + 1;
            fallback.source_count = (uint32_t)node->sources.count;
            FS98_HOST_STRCPY(HOST)(fallback.name_and_sources, fallback_name);
            FS98_HOST_MEMMOVE(HOST)(
                (uint8_t *)fallback.name_and_sources + fallback.name_size,
                node->sources.data, source_size);
            fallback.payload_size = fallback.name_size + source_size + 0x0c;
            total_size = fallback.name_size + source_size + 0x20;
            if (facil_write_node(file, &fallback, total_size, level, NULL) != 0)
                return -1;
            if (FS98_HOST_FREE(HOST)((void **)&section) != 0)
                return -1;
        }
    }
    return 0;
}

static int32_t facil_write_node(struct fs98_host_file *file,
                                struct facil_disk_node *record,
                                uint32_t size, uint32_t level,
                                struct facil_node *node)
{
    uint32_t position;
    uint32_t wrote;

    if (HOST_FILE_TELL(file, &position) != 0)
        return -1;
    if (LAST_OFFSETS[level] != 0 &&
        facil_backfill(file, LAST_OFFSETS[level] + 4, position) != 0)
        return -1;
    LAST_OFFSETS[level] = position;
    if (HOST_FILE_WRITE(file, record, size, &wrote) != 0 || wrote != size)
        return -1;
    {
        uint32_t child_position = 0;
        int has_children = level != 0 && node != NULL && node->children_ready;

        if (has_children && HOST_FILE_TELL(file, &child_position) != 0)
            return -1;
        /* Retail backfills this field even when it is already zero.  Besides
         * being visible to the harness, that keeps the file cursor/error
         * behavior identical on a short or otherwise broken write. */
        if (facil_backfill(file, position + 8, child_position) != 0)
            return -1;
        if (has_children &&
            facil_write_tree(file, &node->children, position, level - 1) != 0)
            return -1;
    }
    return 0;
}

static int32_t facil_replace_index(const char *destination,
                                   const char *source)
{
    int32_t attributes;

    if (FS98_HOST_EXISTS(HOST)(destination, 0) == 0) {
        if (HOST_GET_ATTRIBUTES(&attributes, destination, 0) != 0 ||
            HOST_SET_ATTRIBUTES(1, destination, 0) != 0 ||
            HOST_FILE_DELETE(destination, 0) != 0)
            return -1;
    } else {
        attributes = 1;
    }
    if (HOST_FILE_MOVE(destination, 0, source, 0) != 0 ||
        HOST_SET_ATTRIBUTES(attributes, destination, 0) != 0)
        return -1;
    return 0;
}

static int32_t facil_serialize(const uint8_t *ids, uint32_t count)
{
    struct fs98_host_file *file;
    uint32_t header[3] = { 0, 0, 0 };
    uint32_t wrote;
    uint32_t position;
    char temporary[0x104];
    char destination[0x104];

    FS98_HOST_MEMSET(HOST)(LAST_OFFSETS, 0, 0x14);
    FS98_HOST_LOADSTRING(HOST)(0x9101, destination, sizeof(destination));
    if (HOST_TEMP(temporary, 0) != 0 ||
        HOST_FILE_CREATE(&file, temporary, 0) != 0 ||
        HOST_FILE_WRITE(file, header, sizeof(header), &wrote) != 0 ||
        wrote != sizeof(header))
        return -1;
    if (facil_backfill(file, 8, count) != 0 ||
        HOST_FILE_WRITE(file, ids, count * 16, &wrote) != 0 ||
        wrote != count * 16)
        return -1;
    if (HOST_FILE_SIZE(file, &position) != 0 ||
        facil_backfill(file, 4, position) != 0 ||
        facil_write_tree(file, ROOT_TREE, 0, 4) != 0 ||
        HOST_FILE_SIZE(file, &position) != 0 ||
        facil_backfill(file, 0, position) != 0 ||
        FS98_HOST_FILE_CLOSE(HOST)(file) != 0)
        return -1;
    return facil_replace_index(destination, temporary);
}

static int32_t facil_build_index(void)
{
    struct fs98_host_list blocks;
    uint8_t *ids = NULL;
    uint32_t count = 0;
    uint32_t index;
    uint32_t start;
    int32_t result = 0;
    void *alert = NULL;

    UTIL_ALERT(&alert, 1, 0xf570, 0, 0, 0, 0, 0);
    start = FS98_HOST_TICKS(HOST)();
    blocks.count = 0;
    blocks.capacity = 0;
    blocks.grow = 0x20;
    blocks.elem_size = 4;
    blocks.iterating = 0;
    blocks.pending = 0;
    blocks.data = NULL;
    if (facil_reset_tree() != 0 || !FE_LIST(&ids, &count))
        result = -1;
    if (result == 0) {
        facil_sort_ids(ids, count);
        facil_unique_ids(ids, &count);
        for (index = count; index != 0; index--) {
            uint8_t *block = NULL;
            uint8_t **stored;

            ACTIVE_ID = ids + (index - 1) * 16;
            if (!FE_OPEN(ACTIVE_ID, &block) || facil_parse_block(block) != 0) {
                result = -1;
                break;
            }
            stored = (uint8_t **)FS98_HOST_LIST_ADD(HOST)(&blocks);
            *stored = block;
        }
    }
    if (result == 0 && facil_serialize(ids, count) != 0)
        result = -1;
    if (FS98_HOST_FREE(HOST)((void **)&ids) != 0)
        result = -1;
    for (index = (uint32_t)blocks.count; index != 0; index--)
        if (FS98_HOST_FREE(HOST)(
                (void **)((uint8_t *)blocks.data + (index - 1) * 4)) != 0)
            result = -1;
    if (blocks.data != NULL && FS98_HOST_FREE(HOST)(&blocks.data) != 0)
        result = -1;
    if (facil_reset_tree() != 0)
        result = -1;
    while (FS98_HOST_TICKS(HOST)() < start + 0xfa0)
        Sleep(0);
    if (alert != NULL)
        UTIL_ALERT_CLOSE(&alert);
    return result;
}

static uint32_t FS98_STDCALL facil_rebuild(void)
{
    uint8_t *old_ids = NULL;
    uint8_t *new_ids = NULL;
    uint32_t old_count = 0;
    uint32_t new_count = 0;
    int32_t result = 0;

    if (D32(0x2d08) != 0)
        return 0xffffffffu;
    if (facil_read_old_ids(&old_ids, &old_count) != 0)
        return 0xffffffffu;
    if (!FE_LIST(&new_ids, &new_count))
        return 0xffffffffu;
    facil_sort_ids(new_ids, new_count);
    facil_unique_ids(new_ids, &new_count);
    if (facil_index_changed(old_ids, old_count, new_ids, new_count) &&
        facil_build_index() != 0)
        result = -1;
    if (FS98_HOST_FREE(HOST)((void **)&old_ids) != 0 ||
        FS98_HOST_FREE(HOST)((void **)&new_ids) != 0)
        result = -1;
    return (uint32_t)result;
}

/* .text+0x5540 -- copy one serialized cache record for a caller that already
 * selected its file offset.  The search/list services own the mapping's
 * lifetime; this slot deliberately does not open INDEX_FILE itself. */
static uint32_t FS98_STDCALL facil_copy_record(uint32_t offset,
                                               uint8_t **record,
                                               uint32_t *size)
{
    uint8_t *source = (uint8_t *)(uintptr_t)D32(0x2b58) + offset;
    uint32_t record_size = *(facil_u32 *)(source + 0x10) + 0x14;

    *record = NULL;
    *size = 0;
    if (offset != 0) {
        if (FS98_HOST_ALLOC(HOST)(1, record_size, (void **)record, 0) != 0)
            return 0xffffffffu;
        if (FS98_HOST_MEMMOVE(HOST)(*record, source, record_size) != 0)
            return 0xffffffffu;
        *size = record_size;
    }
    return 0;
}

static uint32_t FS98_STDCALL facil_init(void)
{
    FS98_HOST_CFG_STRING(HOST)((const char *)D + 0x4f0,
                               (const char *)D + 0x53c,
                               (char *)D + 0x2b60, 0x64);
    FS98_HOST_CFG_STRING(HOST)((const char *)D + 0x4f0,
                               (const char *)D + 0x534,
                               (char *)D + 0x2bc8, 0x64);
    FS98_HOST_CFG_STRING(HOST)((const char *)D + 0x4f0,
                               (const char *)D + 0x52c,
                               (char *)D + 0x2c30, 0x64);
    FS98_HOST_CFG_STRING(HOST)((const char *)D + 0x4f0,
                               (const char *)D + 0x524,
                               (char *)D + 0x2c98, 0x64);
    HOST_CFG_INT((const char *)D + 0x4f0, (const char *)D + 0x518,
                 (int32_t *)(D + 0x4ec));
    HOST_CFG_INT((const char *)D + 0x4f0, (const char *)D + 0x4fc,
                 (int32_t *)(D + 0x2d00));
    return 0;
}

static uint32_t FS98_STDCALL facil_deinit(void)
{
    if (D32(0x2d08) != 0)
        return 0xffffffffu;
    HOST_CFG_PUT_STRING((const char *)D + 0x4f0, (const char *)D + 0x53c,
                        (const char *)D + 0x2b60);
    HOST_CFG_PUT_STRING((const char *)D + 0x4f0, (const char *)D + 0x534,
                        (const char *)D + 0x2bc8);
    HOST_CFG_PUT_STRING((const char *)D + 0x4f0, (const char *)D + 0x52c,
                        (const char *)D + 0x2c30);
    HOST_CFG_PUT_STRING((const char *)D + 0x4f0, (const char *)D + 0x524,
                        (const char *)D + 0x2c98);
    HOST_CFG_PUT_INT((const char *)D + 0x4f0, (const char *)D + 0x518,
                     (int32_t)D32(0x4ec));
    return 0;
}

static uint32_t FS98_STDCALL facil_unimplemented(void)
{
    return 0xffffffffu;
}

struct facil_image facil_image = {
    {
        { FS98_MOD_HOST,    NULL },
        { FS98_MOD_GLOBAL,  NULL },
        { FS98_MOD_CHAIN,   NULL },
        { FS98_MOD_FE,      NULL },
        { FS98_MOD_MAIN,    NULL },
        { FS98_MOD_NUMERIC, NULL },
        { FS98_MOD_UTIL,    NULL },
        { 0,                NULL }
    },
    { FS98_MOD_FACIL, NULL, NULL, 0, 0, NULL, { NULL } },
    {
#include "facil_data.inc"
    }
};

__asm__(".globl _ImportTable\n\t.set _ImportTable, _facil_image\n\t"
        ".globl _Linkage\n\t.set _Linkage, _facil_image + 0x40");

static void facil_install_pointers(void)
{
#define FACIL_RELOCS
#define FACIL_REL_DATA(offset, target) \
    D32(offset) = (uint32_t)(uintptr_t)(D + (target));
#define FACIL_REL_TEXT(offset, target) \
    D32(offset) = (uint32_t)(uintptr_t)facil_unimplemented;
#include "facil_data.inc"
#undef FACIL_REL_TEXT
#undef FACIL_REL_DATA
#undef FACIL_RELOCS

    facil_image.linkage.init = facil_init;
    facil_image.linkage.deinit = facil_deinit;
    facil_image.linkage.slots[0] = (facil_fn)facil_unimplemented;
    facil_image.linkage.slots[1] = (facil_fn)facil_unimplemented;
    facil_image.linkage.slots[2] = (facil_fn)facil_copy_record;
    facil_image.linkage.slots[3] = (facil_fn)facil_unimplemented;
    facil_image.linkage.slots[4] = (facil_fn)facil_unimplemented;
    facil_image.linkage.slots[5] = (facil_fn)facil_rebuild;
    facil_image.linkage.slots[6] = (facil_fn)facil_unimplemented;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        facil_install_pointers();
    }
    return TRUE;
}
