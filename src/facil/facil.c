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

/* ---- the mapped cache: .text+0x5680 and +0x57e0 -------------------------
 *
 * +0x18 maps INDEX_FILE on first use and leaves it mapped on success; +0x1c
 * and +0x20 read through that mapping without mapping it themselves.  Retail
 * keeps three copies of the view base.  Node offsets are file offsets, and a
 * zero offset reads the header, exactly as in retail. */

#define MAP_FIRST            D32(0x2b50)
#define MAP_HEADER           D32(0x2b54)
#define MAP_VIEW             D32(0x2b58)
#define MAP_EMPTY            D32(0x2b5c)
#define QUEUE                DPTR(0x2d04, struct facil_queue)
#define MAPPED               D32(0x2d08)
#define MAP_FILE             D32(0x2d0c)
#define MAP_MAPPING          D32(0x2d10)

#define VIEW                 ((uint8_t *)(uintptr_t)MAP_VIEW)
#define NODE(offset, field)  (*(facil_u32 *)(VIEW + (offset) + (field)))
#define NODE_PARENT          0x00
#define NODE_SIBLING         0x04
#define NODE_CHILD           0x08
#define NODE_KIND            0x0c
#define NODE_FLAGS           0x14
#define NODE_NAME(offset)    ((char *)VIEW + (offset) + 0x20)
#define LEVEL(kind)          (((kind) - 0x10000) >> 16)
#define LEVEL_BITS(kind)     (((kind) - 0x10000) & 0xffff0000)

typedef int32_t (FS98_STDCALL *facil_basedir_fn)(char *);
#define HOST_BASEDIR          SLOT(HOST, 0x130, facil_basedir_fn)

/* A caller's filter: a kind (0 in the low word matches the whole level) and
 * an optional name. */
struct facil_filter {
    uint32_t kind;
    const char *name;
};

struct facil_filter_list {
    uint32_t count;
    struct facil_filter *entries;
};

/* What both query services return: 12-byte records, names owned by the
 * caller. */
struct facil_result {
    char *name;
    uint32_t kind;
    uint32_t node;
};

struct facil_results {
    uint32_t count;
    struct facil_result *records;
};

/* A C++ priority queue of result pointers, 1-based: `data` is kept one slot
 * below the host allocation. */
struct facil_queue {
    uint32_t capacity;
    uint32_t grow;
    uint32_t count;
    struct facil_result **data;
    uint32_t flags;
};

typedef char (*facil_query_check)[
    sizeof(struct facil_filter) == 8 &&
    sizeof(struct facil_result) == 0xc &&
    sizeof(struct facil_queue) == 0x14 ? 1 : -1];

/* Retail allocates the queue with MSVCRT's operator new/delete.  mingw's
 * import library does not carry those mangled names, so they are bound by
 * name on first use; the routines are the ones retail calls. */
typedef void *(__cdecl *facil_new_fn)(uint32_t);
typedef void (__cdecl *facil_delete_fn)(void *);
static facil_new_fn facil_new;
static facil_delete_fn facil_delete;

static void facil_bind_crt(void)
{
    HMODULE crt;

    if (facil_new != NULL)
        return;
    crt = LoadLibraryA("MSVCRT.dll");
    facil_new = (facil_new_fn)(void *)GetProcAddress(crt, "??2@YAPAXI@Z");
    facil_delete = (facil_delete_fn)(void *)GetProcAddress(crt,
                                                     "??3@YAXPAX@Z");
}

/* .text+0x5680 */
static int32_t facil_map(void)
{
    char directory[0x108];
    char name[0x100];
    char path[0x104];
    HANDLE file;
    HANDLE mapping;
    uint8_t *header;

    if (MAPPED == 0) {
        FS98_HOST_LOADSTRING(HOST)(0x9101, name, 0x100);
        if (HOST_BASEDIR(directory) != 0)
            return -1;
        if (FS98_HOST_PATH_JOIN(HOST)(path, NULL, directory, name) != 0)
            return -1;
        file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                           FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_FLAG_RANDOM_ACCESS | FILE_ATTRIBUTE_NORMAL,
                           NULL);
        MAP_FILE = (uint32_t)(uintptr_t)file;
        if (file == INVALID_HANDLE_VALUE)
            return -1;
        mapping = CreateFileMappingA(file, NULL, PAGE_READWRITE, 0, 0, NULL);
        MAP_MAPPING = (uint32_t)(uintptr_t)mapping;
        if (mapping == NULL) {
            CloseHandle((HANDLE)(uintptr_t)MAP_FILE);
            return -1;
        }
        MAP_VIEW = (uint32_t)(uintptr_t)MapViewOfFile(mapping, FILE_MAP_READ,
                                                      0, 0, 0);
        /* Retail closes both handles on a failed view but still marks the
         * cache mapped and reads its header through the null view. */
        if (MAP_VIEW == 0) {
            CloseHandle((HANDLE)(uintptr_t)MAP_MAPPING);
            CloseHandle((HANDLE)(uintptr_t)MAP_FILE);
        }
        MAPPED = 1;
        MAP_HEADER = MAP_VIEW;
        MAP_FIRST = MAP_VIEW;
    }
    header = (uint8_t *)(uintptr_t)MAP_HEADER;
    MAP_EMPTY = *(facil_u32 *)header == *(facil_u32 *)(header + 4);
    return 0;
}

/* .text+0x57e0.  The three view copies are left stale. */
static int32_t facil_unmap(void)
{
    if (MAPPED != 0) {
        if (!UnmapViewOfFile((void *)(uintptr_t)MAP_VIEW))
            return -1;
        if (!CloseHandle((HANDLE)(uintptr_t)MAP_MAPPING))
            return -1;
        if (!CloseHandle((HANDLE)(uintptr_t)MAP_FILE))
            return -1;
        MAPPED = 0;
    }
    return 0;
}

/* ---- the result queue: .text+0x5840..+0x5dd0 and +0x6910 ---------------- */

/* .text+0x5840.  Bits other than 0 and 1 of `flags` keep whatever operator
 * new returned. */
static int32_t facil_queue_create(void)
{
    struct facil_queue *queue;
    uint32_t flags;

    facil_bind_crt();
    queue = (struct facil_queue *)facil_new(0x14);
    if (queue != NULL) {
        flags = queue->flags;
        queue->capacity = 0x200;
        queue->grow = 0x80;
        queue->count = 0;
        queue->data = NULL;
        queue->flags = (flags & ~2u) | 1;
        FS98_HOST_ALLOC(HOST)(1, 0x800, (void **)&queue->data, 0);
        queue->data--;
    }
    QUEUE = queue;
    /* Retail then clears bit 1 when bit 0, sign-extended, equals 1.  That
     * never holds, so only the read (which faults on a failed new) remains. */
    flags = *(volatile uint32_t *)&queue->flags;
    (void)flags;
    return 0;
}

/* .text+0x58e0.  Bit 1 (owning) is always clear, so records are never deleted
 * here; the pointer is left stale. */
static int32_t facil_queue_destroy(void)
{
    struct facil_queue *queue = QUEUE;
    uint32_t count;

    if (queue != NULL) {
        if (queue->flags & 1) {
            queue->data++;
            if (queue->flags & 2) {
                for (;;) {
                    count = queue->count;
                    queue->count = count - 1;
                    if (count == 0)
                        break;
                    facil_delete(queue->data[count - 1]);
                }
            }
            FS98_HOST_FREE(HOST)((void **)&queue->data);
        }
        facil_delete(queue);
    }
    return 0;
}

/* .text+0x5dd0 */
static int facil_name_greater(const struct facil_result *left,
                              const struct facil_result *right)
{
    return FS98_HOST_STRICMP(HOST)(left->name, right->name) > 0;
}

/* .text+0x6910 */
static void facil_sift_down(struct facil_queue *queue, uint32_t index)
{
    for (;;) {
        uint32_t left = index * 2;
        uint32_t right = index * 2 + 1;
        uint32_t best = index;
        struct facil_result *swap;

        if (left <= queue->count &&
            facil_name_greater(queue->data[index], queue->data[left]))
            best = left;
        if (right <= queue->count &&
            facil_name_greater(queue->data[best], queue->data[right]))
            best = right;
        if (best == index)
            return;
        swap = queue->data[index];
        queue->data[index] = queue->data[best];
        queue->data[best] = swap;
        index = best;
    }
}

/* Inlined three times in .text+0x5aa0. */
static struct facil_result *facil_queue_pop(struct facil_queue *queue)
{
    struct facil_result *top;
    struct facil_result *swap;
    uint32_t best;

    if (queue->count == 0)
        return NULL;
    top = queue->data[1];
    queue->data[1] = queue->data[queue->count];
    queue->count--;
    best = 1;
    if (queue->count >= 2 &&
        facil_name_greater(queue->data[1], queue->data[2]))
        best = 2;
    if (queue->count >= 3 &&
        facil_name_greater(queue->data[best], queue->data[3]))
        best = 3;
    if (best != 1) {
        swap = queue->data[1];
        queue->data[1] = queue->data[best];
        queue->data[best] = swap;
        facil_sift_down(queue, best);
    }
    return top;
}

/* .text+0x5960 -- copy a node's name, kind and offset into the queue. */
static int32_t facil_queue_insert(uint32_t node)
{
    struct facil_result *record;
    struct facil_queue *queue;
    struct facil_result *parent;
    uint32_t index;
    uint32_t above;

    if (FS98_HOST_ALLOC(HOST)(1, 0xc, (void **)&record, 0) != 0)
        return -1;
    if (FS98_HOST_ALLOC(HOST)(1, FS98_HOST_STRLEN(HOST)(NODE_NAME(node)) + 1,
                              (void **)&record->name, 0) != 0)
        return -1;
    FS98_HOST_STRCPY(HOST)(record->name, NODE_NAME(node));
    record->node = node;
    record->kind = NODE(node, NODE_KIND);
    queue = QUEUE;
    if (queue->count == queue->capacity) {
        queue->capacity = queue->grow + queue->capacity;
        queue->data++;
        FS98_HOST_REALLOC(HOST)(1, queue->capacity << 2,
                                (void **)&queue->data);
        queue->data--;
    }
    index = ++queue->count;
    while (index > 1) {
        above = index >> 1;
        parent = queue->data[above];
        if (FS98_HOST_STRICMP(HOST)(record->name, parent->name) > 0)
            break;
        queue->data[index] = parent;
        index = above;
    }
    queue->data[index] = record;
    return 0;
}

/* .text+0x5aa0 -- empty the queue into `out` in name order.  With `dedupe`,
 * a record whose name equals the last one kept (case-insensitively) is
 * dropped, name and all. */
static int32_t facil_queue_drain(struct facil_results *out, uint32_t dedupe)
{
    struct facil_result *record;
    uint32_t written = 0;
    uint32_t offset;

    out->count = 0;
    if (QUEUE->count == 0) {
        out->records = NULL;
        out->count = 0;
        return 0;
    }
    if (FS98_HOST_ALLOC(HOST)(1, QUEUE->count * 0xc, (void **)&out->records,
                              0) != 0)
        return -1;
    record = facil_queue_pop(QUEUE);
    if (record != NULL) {
        offset = 0;
        do {
            if (FS98_HOST_MEMMOVE(HOST)((uint8_t *)out->records + offset,
                                        record, 0xc) != 0)
                return -1;
            if (FS98_HOST_FREE(HOST)((void **)&record) != 0)
                return -1;
            record = facil_queue_pop(QUEUE);
            written++;
            offset += 0xc;
            if (dedupe == 0)
                continue;
            while (record != NULL &&
                   FS98_HOST_STRICMP(HOST)(
                       record->name,
                       *(char **)((uint8_t *)out->records + offset - 0xc))
                       == 0) {
                if (FS98_HOST_FREE(HOST)((void **)record) != 0 ||
                    FS98_HOST_FREE(HOST)((void **)&record) != 0)
                    return -1;
                record = facil_queue_pop(QUEUE);
            }
        } while (record != NULL);
    }
    out->count = written;
    return 0;
}

/* ---- filters: .text+0x5e00..+0x6510 ------------------------------------- */

/* .text+0x5e00 -- descend the first-child chain to the filter's level. */
static uint32_t facil_first_at_level(uint32_t kind)
{
    uint32_t node = *(facil_u32 *)((uint8_t *)(uintptr_t)MAP_HEADER + 4);
    uint32_t want;

    if (node != 0) {
        want = LEVEL_BITS(kind);
        do {
            if (LEVEL_BITS(NODE(node, NODE_KIND)) >= want)
                break;
            node = NODE(node, NODE_CHILD);
        } while (node != 0);
    }
    return node;
}

/* .text+0x61a0 -- kind level, exact kind when the low word is set, and the
 * whole name case-insensitively when the entry has one. */
static uint32_t facil_entry_matches(const struct facil_filter *entry,
                                    uint32_t node)
{
    uint32_t node_kind = NODE(node, NODE_KIND);
    uint32_t kind = entry->kind;

    if (((kind - 0x10000) ^ (node_kind - 0x10000)) & 0xffff0000)
        return 0;
    if ((kind & 0xffff) != 0 && kind != node_kind)
        return 0;
    if (entry->name != NULL && FS98_HOST_STRLEN(HOST)(entry->name) != 0 &&
        FS98_HOST_STRICMP(HOST)(NODE_NAME(node), entry->name) != 0)
        return 0;
    return 1;
}

/* .text+0x60d0 -- as above, but the name is a case-sensitive prefix. */
static uint32_t facil_filter_matches(const struct facil_filter *filter,
                                     uint32_t node)
{
    uint32_t node_kind = NODE(node, NODE_KIND);
    uint32_t kind = filter->kind;
    uint32_t length;

    if (((kind - 0x10000) ^ (node_kind - 0x10000)) & 0xffff0000)
        return 0;
    if ((kind & 0xffff) != 0 && kind != node_kind)
        return 0;
    if (filter->name == NULL)
        return 1;
    length = FS98_HOST_STRLEN(HOST)(filter->name);
    if (length == 0)
        return 1;
    if (FS98_HOST_STRLEN(HOST)(NODE_NAME(node)) < length)
        return 0;
    return FS98_HOST_MEMCMP(HOST)(NODE_NAME(node), filter->name,
                                  FS98_HOST_STRLEN(HOST)(filter->name)) == 0;
}

/* .text+0x5e90 -- every filter entry above the node's level must match the
 * node's ancestor at that level. */
static uint32_t facil_ancestors_match(const struct facil_filter_list *list,
                                      uint32_t node)
{
    uint32_t level = LEVEL(NODE(node, NODE_KIND));
    uint32_t index = 0;
    uint32_t remaining;
    uint32_t entry_level;
    uint32_t node_level;
    const struct facil_filter *entry;

    if (list->count != 0) {
        while (LEVEL(list->entries[index].kind) < level) {
            index++;
            if (index >= list->count)
                break;
        }
    }
    if (index == 0) {
        entry_level = LEVEL(list->entries[0].kind);
        if (entry_level > level)
            return 1;
        if (entry_level == level)
            return facil_entry_matches(&list->entries[0], node);
    }
    remaining = index;
    entry = &list->entries[index - 1];
    for (;;) {
        entry_level = LEVEL(entry->kind);
        node_level = LEVEL(NODE(node, NODE_KIND));
        while (node_level > entry_level) {
            node = NODE(node, NODE_PARENT);
            node_level = LEVEL(NODE(node, NODE_KIND));
        }
        if (!facil_entry_matches(entry, node))
            return 0;
        entry--;
        if (--remaining == 0)
            return 1;
    }
}

/* .text+0x6010 -- does some node in this sibling run (or below it) match
 * entries[index..]?  Siblings are followed only while their parent is
 * `limit`; the top-level call passes 0, so it checks just the one node. */
static uint32_t facil_subtree_match(const struct facil_filter_list *list,
                                    uint32_t limit, uint32_t node,
                                    uint32_t index)
{
    uint32_t entry_level;

    if (node == 0)
        return 0;
    entry_level = LEVEL(list->entries[index].kind);
    for (;;) {
        if (LEVEL(NODE(node, NODE_KIND)) == entry_level) {
            if (facil_entry_matches(&list->entries[index], node)) {
                if (index == list->count - 1)
                    return 1;
                if (facil_subtree_match(list, node, NODE(node, NODE_CHILD),
                                        index + 1))
                    return 1;
            }
        } else if (facil_subtree_match(list, node, NODE(node, NODE_CHILD),
                                       index)) {
            return 1;
        }
        node = NODE(node, NODE_SIBLING);
        if (node == 0 || NODE(node, NODE_PARENT) == 0 ||
            NODE(node, NODE_PARENT) != limit)
            return 0;
    }
}

/* .text+0x5f80 -- the filter entries at and below the node's level must be
 * satisfied by the node or its descendants. */
static uint32_t facil_descendants_match(const struct facil_filter_list *list,
                                        uint32_t node)
{
    uint32_t level = LEVEL(NODE(node, NODE_KIND));
    uint32_t count = list->count;
    uint32_t index = 0;
    uint32_t result;

    if (count != 0) {
        while (LEVEL(list->entries[index].kind) < level) {
            index++;
            if (index >= count)
                break;
        }
    }
    if (index == count)
        return 1;
    if (LEVEL(list->entries[index].kind) == level)
        result = facil_subtree_match(list, 0, node, index);
    else
        result = facil_subtree_match(list, node, NODE(node, NODE_CHILD),
                                     index);
    return result != 0 ? 1 : 0;
}

/* .text+0x5e50.  Both halves always run. */
static uint32_t facil_matches_filters(const struct facil_filter_list *list,
                                      uint32_t node)
{
    if (list != NULL && list->count != 0)
        return facil_ancestors_match(list, node) &
               facil_descendants_match(list, node);
    return 1;
}

/* .text+0x6440 */
static uint32_t facil_entry_unnamed(const struct facil_filter *entry)
{
    if (entry->name == NULL)
        return 1;
    return FS98_HOST_STRLEN(HOST)(entry->name) == 0;
}

/* .text+0x6470 */
static int32_t facil_list_copy(struct facil_filter_list *destination,
                               const struct facil_filter_list *source)
{
    if (destination != NULL && source != NULL) {
        destination->count = source->count;
        if (FS98_HOST_ALLOC(HOST)(1, source->count << 3,
                                  (void **)&destination->entries, 0) != 0)
            return -1;
        if (FS98_HOST_MEMMOVE(HOST)(destination->entries, source->entries,
                                    source->count << 3) != 0)
            return -1;
    }
    return 0;
}

/* .text+0x6510 */
static int32_t FS98_STDCALL facil_filter_compare(const void *left,
                                                 const void *right)
{
    uint32_t a = *(const facil_u32 *)left;
    uint32_t b = *(const facil_u32 *)right;

    if (a > b)
        return 1;
    return a != b ? -1 : 0;
}

/* .text+0x64e0 */
static int32_t facil_list_sort(struct facil_filter_list *list)
{
    FS98_HOST_QSORT(HOST)(list->entries, list->count, 8,
                          facil_filter_compare);
    return 0;
}

/* .text+0x6370 -- replace `list` with the named entries of `source`, in
 * source order.  The sorted copy the caller just made is freed unused. */
static int32_t facil_named_filters(const struct facil_filter_list *source,
                                   const struct facil_filter *unused,
                                   struct facil_filter_list *list)
{
    struct fs98_host_list named = { 0, 0, 0x20, 8, 0, 0, NULL };
    uint32_t index;
    void *slot;

    (void)unused;
    list->count = 0;
    if (source != NULL) {
        for (index = 0; index < source->count; index++) {
            if (facil_entry_unnamed(&source->entries[index]))
                continue;
            slot = FS98_HOST_LIST_ADD(HOST)(&named);
            if (slot == NULL ||
                FS98_HOST_MEMMOVE(HOST)(slot, &source->entries[index], 8) != 0)
                return -1;
        }
    }
    if (list->entries != NULL)
        FS98_HOST_FREE(HOST)((void **)&list->entries);
    list->entries = (struct facil_filter *)named.data;
    list->count = (uint32_t)named.count;
    return 0;
}

/* ---- +0x1c's helpers: .text+0x6540..+0x6770 ----------------------------- */

/* .text+0x6770 -- step over a run of flag-2 nodes (an airport and the name
 * record serialized after it).  Returns 1 when `target` is in the run. */
static uint32_t facil_group_step(uint32_t node, uint32_t target,
                                 uint32_t *next)
{
    if (node != target) {
        do {
            if (!(VIEW[node + NODE_FLAGS] & 2)) {
                *next = NODE(node, NODE_SIBLING);
                return 0;
            }
            node = NODE(node, NODE_SIBLING);
        } while (node != target);
    }
    *next = node;
    return 1;
}

/* .text+0x6710 -- the first node of the run containing `node`. */
static uint32_t facil_group_start(uint32_t node)
{
    uint32_t start;
    uint32_t next;

    if (LEVEL_BITS(NODE(node, NODE_KIND)) == 0)
        return *(facil_u32 *)((uint8_t *)(uintptr_t)MAP_HEADER + 4);
    start = NODE(NODE(node, NODE_PARENT), NODE_CHILD);
    while (start < node) {
        if (facil_group_step(start, node, &next))
            break;
        start = next;
    }
    return start;
}

/* .text+0x6610 -- climb from `start` to the entry's level and take the last
 * matching node of that run.  A second match leaks the first name. */
static int32_t facil_resolve(uint32_t start, const struct facil_filter *entry,
                             struct facil_result *record)
{
    uint32_t node = start;
    uint32_t want;

    record->name = NULL;
    record->kind = 0;
    record->node = 0;
    want = LEVEL_BITS(entry->kind);
    if (want > LEVEL_BITS(NODE(node, NODE_KIND)) || node == 0)
        return 0;
    while (LEVEL_BITS(NODE(node, NODE_KIND)) > want) {
        node = NODE(node, NODE_PARENT);
        if (node == 0)
            return 0;
    }
    for (;;) {
        if (!(VIEW[node + NODE_FLAGS] & 1) &&
            facil_entry_matches(entry, node)) {
            if (FS98_HOST_ALLOC(HOST)(
                    1, FS98_HOST_STRLEN(HOST)(NODE_NAME(node)) + 1,
                    (void **)&record->name, 0) != 0)
                return -1;
            FS98_HOST_STRCPY(HOST)(record->name, NODE_NAME(node));
            record->node = node;
            record->kind = NODE(node, NODE_KIND);
        }
        if (!(VIEW[node + NODE_FLAGS] & 2))
            return 0;
        node = NODE(node, NODE_SIBLING);
        if (node == 0)
            return 0;
    }
}

/* .text+0x6540 */
static int32_t facil_collect(uint32_t start,
                             const struct facil_filter_list *filters,
                             struct facil_results *out)
{
    struct fs98_host_list results = { 0, 0, 0x20, 0xc, 0, 0, NULL };
    struct facil_result record;
    uint32_t written = 0;
    uint32_t index;
    void *slot;

    out->count = 0;
    for (index = 0; index < filters->count; index++) {
        if (facil_resolve(start, &filters->entries[index], &record) != 0)
            return -1;
        if (record.name == NULL)
            continue;
        slot = FS98_HOST_LIST_ADD(HOST)(&results);
        if (slot == NULL ||
            FS98_HOST_MEMMOVE(HOST)(slot, &record, 0xc) != 0)
            return -1;
        written++;
    }
    out->count = written;
    out->records = (struct facil_result *)results.data;
    return 0;
}

/* .text+0x5420 -- Linkage +0x18: every node at `filter`'s level whose name
 * starts with filter's name and which fits the named entries of `filters`,
 * in name order.  Every failure after the zeroing unmaps the cache and, if
 * that succeeds, returns 0 -- retail's error result is the unmap's.  Success
 * leaves the cache mapped for +0x1c and +0x20; failure also leaks the queue
 * and the filter copy. */
static uint32_t FS98_STDCALL facil_search(
    const struct facil_filter_list *filters, const struct facil_filter *filter,
    struct facil_results *out, uint32_t dedupe)
{
    struct facil_filter_list named = { 0, NULL };
    uint32_t node;

    out->count = 0;
    out->records = NULL;
    if (facil_map() != 0 || MAP_EMPTY != 0)
        goto fail;
    if (facil_queue_create() != 0)
        return 0xffffffffu;
    node = facil_first_at_level(filter->kind);
    if (facil_list_copy(&named, filters) != 0 ||
        facil_list_sort(&named) != 0 ||
        facil_named_filters(filters, filter, &named) != 0)
        goto fail;
    for (; node != 0; node = NODE(node, NODE_SIBLING))
        if (facil_matches_filters(&named, node) &&
            facil_filter_matches(filter, node) &&
            facil_queue_insert(node) != 0)
            goto fail;
    if (facil_queue_drain(out, dedupe) != 0 ||
        facil_queue_destroy() != 0 ||
        FS98_HOST_FREE(HOST)((void **)&named.entries) != 0)
        goto fail;
    return 0;
fail:
    return facil_unmap() != 0 ? 0xffffffffu : 0;
}

/* .text+0x55c0 -- Linkage +0x1c: for each entry of `filters`, in caller
 * order, the selected node's ancestor-or-self that matches it.  Reads the
 * mapping +0x18 left behind.  The sorted copy is made and freed unused. */
static uint32_t FS98_STDCALL facil_describe(
    uint32_t node, const struct facil_filter_list *filters,
    struct facil_results *out)
{
    struct facil_filter_list sorted = { 0, NULL };

    out->count = 0;
    out->records = NULL;
    if (facil_list_copy(&sorted, filters) != 0 ||
        facil_list_sort(&sorted) != 0)
        return 0xffffffffu;
    if (facil_collect(facil_group_start(node), filters, out) != 0)
        return 0xffffffffu;
    return FS98_HOST_FREE(HOST)((void **)&sorted.entries) != 0 ?
           0xffffffffu : 0;
}

static uint32_t FS98_STDCALL facil_rebuild(void)
{
    uint8_t *old_ids = NULL;
    uint8_t *new_ids = NULL;
    uint32_t old_count = 0;
    uint32_t new_count = 0;
    int32_t result = 0;

    if (facil_unmap() != 0)
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
    if (facil_unmap() != 0)
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

/* ---- the dialogs: .text+0x1040..+0x3be0 ---------------------------------
 *
 * FACIL has no window code.  Each dialog slot fills a parameter block in
 * .data and runs a dialog that English\World.DLL registers -- 0x1307,
 * 0x1308 and 0x1309 -- through host +0x10c.  World draws the controls and
 * calls back through function pointers in the block, which is all that is
 * here.  Field offsets inside those blocks are written as raw numbers so they
 * can be matched against World's side.
 *
 *   0x1307  facilities  .data+0x80   Linkage +0x30, and a button in the others
 *   0x1308  position    .data+0x158  Linkage +0x28
 *   0x1309  airport     .data+0x3e8  Linkage +0x24
 *
 * Result lists here are { count, records, selection } with the 12-byte
 * records of the query services; some keep an owned payload in the third
 * word of each record.  Retail leaks freely on both success and failure
 * paths; every leak is kept. */

typedef uint16_t __attribute__((may_alias)) facil_u16;

#define P32(base, offset)    (*(facil_u32 *)((uint8_t *)(base) + (offset)))
#define P16(base, offset)    (*(facil_u16 *)((uint8_t *)(base) + (offset)))
#define PTR(value)           ((uint8_t *)(uintptr_t)(value))
#define U32(pointer)         ((uint32_t)(uintptr_t)(pointer))
#define GLOBAL               ((uint8_t *)IMPORT(1))
#define CHAIN                IMPORT(2)
#define MAIN                 IMPORT(4)
#define NUMERIC              IMPORT(5)
#define G32(offset)          P32(GLOBAL, offset)
#define G16(offset)          P16(GLOBAL, offset)
#define G8(offset)           (GLOBAL[offset])
#define D16(offset)          P16(D, offset)
#define D8(offset)           (D[offset])
#define RECORD(list, index)  ((struct facil_result *)PTR(P32(list, 4)) + (index))

typedef void (FS98_STDCALL *facil_void_fn)(void);
typedef void (FS98_STDCALL *facil_void1_fn)(uint32_t);
typedef uint32_t (FS98_STDCALL *facil_chain_fn)(uint32_t, uint32_t, uint32_t);
typedef int32_t (FS98_STDCALL *facil_ground_fn)(void *);
typedef uint32_t (FS98_STDCALL *facil_fe_next_fn)(void **, void **);
typedef void (FS98_STDCALL *facil_fe_at_fn)(uint32_t, uint32_t, uint32_t);
typedef void (FS98_STDCALL *facil_format_fn)(const void *, uint32_t, char *);
typedef int32_t (FS98_STDCALL *facil_parse_fn)(const char *, uint32_t, void *,
                                               const void *);
typedef void (FS98_STDCALL *facil_sincos_fn)(uint32_t, int16_t *, int16_t *);
typedef void (FS98_STDCALL *facil_lonlat_fn)(const void *, const void *,
                                             void *);
typedef void (FS98_STDCALL *facil_marshal_fn)(const void *, const void *,
                                              int32_t);
typedef int32_t (FS98_STDCALL *facil_put_fn)(void *, uint32_t, void *,
                                             int32_t);
typedef void (FS98_STDCALL *facil_get_fn)(void *, uint32_t, void *);
typedef void (FS98_STDCALL *facil_to_double_fn)(double *, const void *);
typedef void (FS98_STDCALL *facil_from_double_fn)(const double *, void *);

#define FE_GROUND             SLOT(FE, 0x30, facil_ground_fn)
#define FE_AREA_NEXT          SLOT(FE, 0x34, facil_fe_next_fn)
#define FE_NAV_NEXT           SLOT(FE, 0x38, facil_fe_next_fn)
#define FE_AREA_FIRST         SLOT(FE, 0x4c, facil_void_fn)
#define FE_NAV_FIRST          SLOT(FE, 0x50, facil_fe_at_fn)
#define FE_EXTRACT            SLOT(FE, 0x88, facil_fe_read_fn)
#define NUM_FORMAT            SLOT(NUMERIC, 0x18, facil_format_fn)
#define NUM_PARSE             SLOT(NUMERIC, 0x20, facil_parse_fn)
#define UTIL_SINCOS           SLOT(UTIL, 0x2c, facil_sincos_fn)
#define UTIL_M32_TO_LON       SLOT(UTIL, 0x40, facil_lonlat_fn)
#define UTIL_LON_TO_M         SLOT(UTIL, 0x48, facil_lonlat_fn)
#define UTIL_MARSHAL          SLOT(UTIL, 0x88, facil_marshal_fn)
#define UTIL_PUT              SLOT(UTIL, 0x8c, facil_put_fn)
#define UTIL_GET              SLOT(UTIL, 0x90, facil_get_fn)
#define UTIL_TO_DOUBLE        SLOT(UTIL, 0xc0, facil_to_double_fn)
#define UTIL_FROM_DOUBLE      SLOT(UTIL, 0xc4, facil_from_double_fn)
#define UTIL_TO_DOUBLE_S      SLOT(UTIL, 0xc8, facil_to_double_fn)
#define CHAIN_DISPATCH        SLOT(CHAIN, 0x20, facil_chain_fn)
#define MAIN_1C               SLOT(MAIN, 0x1c, facil_void_fn)
#define MAIN_B0               SLOT(MAIN, 0xb0, facil_void_fn)
#define MAIN_C4               SLOT(MAIN, 0xc4, facil_void_fn)
/* GLOBAL +0x11d4 is the .SIM module's Linkage. */
#define SIM_1C                SLOT(PTR(G32(0x11d4)), 0x1c, facil_void_fn)
#define SIM_34                SLOT(PTR(G32(0x11d4)), 0x34, facil_void1_fn)

#define HOST_STRCPY           FS98_HOST_STRCPY(HOST)
#define HOST_STRDUP(s)        U32(FS98_HOST_STRDUP(HOST)(s))
#define HOST_FREE(pp)         FS98_HOST_FREE(HOST)((void **)(pp))
#define LIST_D0               ((struct facil_filter_list *)(D + 0xd0))
#define LIST_D100             ((struct facil_filter_list *)(D + 0x100))
#define LIST_2F8              ((struct facil_filter_list *)(D + 0x2f8))
#define LIST_4D8              ((struct facil_filter_list *)(D + 0x4d8))
#define CALLER_OUT                   PTR(D32(0x6d8))

/* MSVCRT's _ftol: truncation towards zero, 64 bits, the x87's indefinite
 * value out of range. */
static int64_t facil_ftol(long double value)
{
    if (!(value >= -9223372036854775808.0L &&
          value < 9223372036854775808.0L))
        return (int64_t)0x8000000000000000LL;
    return (int64_t)value;
}

/* ---- shared row builders ------------------------------------------------ */

/* .text+0x1dd0 -- three BCD digits from bits 27..16, then `digits` more after
 * .data+0x78's separator.  No terminator is written: the row's zeroes end it. */
static int32_t facil_bcd3(char *out, uint32_t value, uint32_t digits)
{
    uint32_t bits = value << 4;
    uint32_t index;
    char *cursor;

    out[0] = 0;
    for (index = 0; index < 3; index++) {
        out[index] = (char)(((bits >> 28) & 0xf) + 0x30);
        bits <<= 4;
    }
    if (digits != 0) {
        out[index] = (char)D8(0x78);
        cursor = out + index + 1;
        do {
            *cursor++ = (char)(((bits >> 28) & 0xf) + 0x30);
            bits <<= 4;
        } while (--digits != 0);
    }
    return 0;
}

/* .text+0x1d70 -- the same from bits 15..4. */
static int32_t facil_bcd4(char *out, uint32_t value, uint32_t digits)
{
    uint32_t bits = value << 16;
    uint32_t index;
    char *cursor;

    out[0] = 0;
    for (index = 0; index < 3; index++) {
        out[index] = (char)(((bits >> 28) & 0xf) + 0x30);
        bits <<= 4;
    }
    if (digits != 0) {
        out[index] = (char)D8(0x78);
        cursor = out + index + 1;
        do {
            *cursor++ = (char)(((bits >> 28) & 0xf) + 0x30);
            bits <<= 4;
        } while (--digits != 0);
    }
    return 0;
}

/* .text+0x1200 -- +0x18 over .data+0xd0's region/country/state entries. */
static int32_t facil_search_at(uint32_t region, uint32_t country,
                               uint32_t state, struct facil_filter *filter,
                               struct facil_results *out)
{
    D32(0xbc) = region;
    D32(0xc4) = country;
    D32(0xcc) = state;
    return facil_search(LIST_D0, filter, out, 0) != 0 ? -1 : 0;
}

/* .text+0x12e0 -- a row's four place names and its own name.  +0x1c's
 * results at .data+0x550 are never freed. */
static int32_t facil_row_names(uint8_t *row, const struct facil_result *record)
{
    uint32_t index;

    if (FS98_HOST_MEMSET(HOST)(row, 0, 0x14) != 0)
        return -1;
    if (facil_describe(record->node, LIST_D100,
                       (struct facil_results *)(D + 0x550)) != 0)
        return -1;
    for (index = 0; index < D32(0x550); index++) {
        struct facil_result *place = RECORD(D + 0x550, index);

        switch (place->kind) {
        case 0x10001: P32(row, 0x0) = HOST_STRDUP(place->name); break;
        case 0x20001: P32(row, 0x4) = HOST_STRDUP(place->name); break;
        case 0x30001: P32(row, 0x8) = HOST_STRDUP(place->name); break;
        case 0x40001: P32(row, 0xc) = HOST_STRDUP(place->name); break;
        }
    }
    P32(row, 0x10) = HOST_STRDUP(record->name);
    return 0;
}

/* .text+0x13f0 -- latitude, longitude and altitude from section 0x10001. */
static int32_t facil_row_position(uint8_t *row, const struct facil_source *source)
{
    uint8_t *section;
    uint32_t size;

    if (!FE_READ(source, source->value, 0x10001, &section, &size))
        return -1;
    if (section != NULL && size != 0) {
        NUM_FORMAT(section + 0x08, 0x15, (char *)row + 0x1f);
        NUM_FORMAT(section + 0x10, 0x14, (char *)row + 0x34);
        NUM_FORMAT(section + 0x18, 0x1004e, (char *)row + 0x49);
    } else {
        HOST_STRCPY((char *)row + 0x1f, (const char *)D + 0x55c);
        HOST_STRCPY((char *)row + 0x34, (const char *)D + 0x55c);
        HOST_STRCPY((char *)row + 0x49, (const char *)D + 0x55c);
    }
    return HOST_FREE(&section) != 0 ? -1 : 0;
}

/* .text+0x14e0 -- the long name from section 0x10006. */
static int32_t facil_row_title(uint8_t *row, const struct facil_source *source)
{
    uint8_t *section;
    uint32_t size;

    if (!FE_READ(source, source->value, 0x10006, &section, &size))
        return -1;
    if (section != NULL && size != 0)
        HOST_STRCPY((char *)row + 0x14, (const char *)section + 8);
    else
        HOST_STRCPY((char *)row + 0x14, (const char *)D + 0x55c);
    return HOST_FREE(&section) != 0 ? -1 : 0;
}

/* .text+0x1570 -- the first type-1 frequency of section 0x20001.  Finding
 * one returns at once and leaks the section. */
static int32_t facil_row_frequency(uint8_t *row,
                                   const struct facil_source *source)
{
    uint8_t *section;
    uint32_t size;
    uint32_t offset;

    if (!FE_READ(source, source->value, 0x20001, &section, &size))
        return -1;
    if (section != NULL && size != 0) {
        offset = 0;
        do {
            if (P32(section + offset, 0xc) == 1)
                return facil_bcd3((char *)row + 0x51,
                                  P32(section + offset, 8), 3) != 0 ? -1 : 0;
            offset = offset + P32(section + offset, 4) + 8;
        } while (offset < size);
    } else {
        HOST_STRCPY((char *)row + 0x51, (const char *)D + 0x55c);
    }
    return HOST_FREE(&section) != 0 ? -1 : 0;
}

/* .text+0x67c0 -- 1 unless the second point lies ahead of the first along
 * `heading`: the dot product of their separation with the heading's sine and
 * cosine.  Longitudes go through +0xc0 between 0x4000000 and 0xc0000000 and
 * through the signed +0xc8 otherwise. */
static uint32_t facil_behind(const void *latitude1, const void *longitude1,
                             uint32_t heading, const void *latitude2,
                             const void *longitude2)
{
    int16_t sine;
    int16_t cosine;
    uint8_t metres1[8];
    uint8_t metres2[8];
    double x1, x2, y1, y2, sine_d, cosine_d;
    uint32_t high;
    long double sum;

    UTIL_SINCOS((0x40000000u - heading) >> 16, &sine, &cosine);
    UTIL_LON_TO_M(longitude1, latitude1, metres1);
    UTIL_LON_TO_M(longitude2, latitude2, metres2);
    sine_d = (double)sine;
    cosine_d = (double)cosine;
    UTIL_TO_DOUBLE_S(&y1, latitude1);
    UTIL_TO_DOUBLE_S(&y2, latitude2);
    high = P32(longitude2, 4);
    if (high > 0x4000000 && high < 0xc0000000u) {
        UTIL_TO_DOUBLE(&x1, metres1);
        UTIL_TO_DOUBLE(&x2, metres2);
    } else {
        UTIL_TO_DOUBLE_S(&x1, metres1);
        UTIL_TO_DOUBLE_S(&x2, metres2);
    }
    sum = ((long double)x2 - x1) * cosine_d + ((long double)y2 - y1) * sine_d;
    return !(sum > 0.0L);
}

/* .text+0x1830 -- the frequency and name of the last type-4 (ILS) 0x10004
 * record following the runway that is not behind it. */
static int32_t facil_runway_ils(uint8_t *end_row, uint8_t *end, uint8_t *runway)
{
    uint32_t found = 0;
    uint32_t frequency = U32(end);
    const char *name = (const char *)runway;
    uint8_t *next = runway + P32(runway, 4) + 8;

    while (next < end && P32(next, 0) == 0x10004) {
        if (facil_behind(runway + 8, runway + 0x10, P32(runway, 0x20),
                         next + 8, next + 0x10) == 0 &&
            P32(next, 0x28) == 4) {
            frequency = P32(next, 0x24);
            name = (const char *)next + 0x38;
            found = 1;
        }
        next = next + P32(next, 4) + 8;
    }
    if (found) {
        facil_bcd3((char *)end_row + 0x14, frequency, 2);
        HOST_STRCPY((char *)end_row + 0x1f, name);
    }
    return 0;
}

/* .text+0x17e0 -- one 0x2c-byte runway end. */
static int32_t facil_runway_end(uint8_t *end_row, uint8_t *end, uint8_t *runway)
{
    P32(end_row, 8) = P32(runway, 0x38);
    NUM_FORMAT(runway + 0x28, 0x1001b, (char *)end_row + 0xc);
    P32(end_row, 0) = P32(runway, 0x34);
    P32(end_row, 4) = P32(runway, 0x30);
    return facil_runway_ils(end_row, end, runway) != 0 ? -1 : 0;
}

/* .text+0x1630 -- both ends of every runway.  The far end is made by turning
 * FE's own record round in place: heading plus 0x80000000, pattern sides
 * 1 and 2 swapped, and the number less 18, wrapped into 1..36. */
static int32_t facil_row_runways(uint8_t *row, const struct facil_source *source)
{
    struct fs98_host_list ends = { 0, 0, 0x20, 0x2c, 0, 0, NULL };
    uint8_t *block;
    uint32_t size;
    uint32_t offset = 0x10;
    uint8_t *runway;
    uint8_t *end_row;

    if (!FE_EXTRACT((const struct facil_source *)source, source->value, 1,
                    &block, &size))
        return -1;
    if (block != NULL && size != 0 && size > 0x10) {
        do {
            runway = block + offset;
            if (P32(runway, 0) != 0x10002) {
                offset = offset + P32(runway, 4) + 8;
                continue;
            }
            end_row = (uint8_t *)FS98_HOST_LIST_ADD(HOST)(&ends);
            if (facil_runway_end(end_row, block + size, runway) != 0)
                return -1;
            end_row = (uint8_t *)FS98_HOST_LIST_ADD(HOST)(&ends);
            P32(runway, 0x20) += 0x80000000u;
            switch (P32(runway, 0x30)) {
            case 1: P32(runway, 0x30) = 2; break;
            case 2: P32(runway, 0x30) = 1; break;
            }
            P32(runway, 0x34) += (uint32_t)-0x12;
            if ((int32_t)P32(runway, 0x34) <= 0)
                P32(runway, 0x34) += 0x24;
            if (facil_runway_end(end_row, block + size, runway) != 0)
                return -1;
            do {
                offset = offset + P32(runway, 4) + 8;
                runway = block + offset;
                if (offset >= size)
                    goto done;
            } while (P32(runway, 0) != 0x10002);
        } while (offset < size);
    }
done:
    P32(row, 0x5c) = (uint32_t)ends.count;
    P32(row, 0x60) = U32(ends.data);
    if (ends.count == 0 && HOST_FREE(row + 0x60) != 0)
        return -1;
    return HOST_FREE(&block) != 0 ? -1 : 0;
}

/* .text+0x1240 -- one 0x64-byte airport row.  The record copy leaks. */
static int32_t facil_airport_row(uint8_t *row, const struct facil_result *found)
{
    uint8_t *record;
    uint32_t size;
    const struct facil_source *source;

    if (facil_copy_record(found->node, &record, &size) != 0)
        return -1;
    if (facil_row_names(row, found) != 0)
        return -1;
    source = (const struct facil_source *)
        (record + P32(record, 0x18) + 0x20);
    if (facil_row_position(row, source) != 0 ||
        facil_row_title(row, source) != 0 ||
        facil_row_frequency(row, source) != 0)
        return -1;
    return facil_row_runways(row, source) != 0 ? -1 : 0;
}

/* .text+0x1a20 -- one 0x60-byte navaid row from section 0x10004.  Bits of
 * +0x5c say which frequency kinds were seen. */
static int32_t facil_navaid_row(uint8_t *row, const struct facil_result *found)
{
    uint8_t *record;
    uint32_t record_size;
    uint8_t *section;
    uint32_t size;
    const struct facil_source *source;
    uint32_t offset;

    if (facil_copy_record(found->node, &record, &record_size) != 0)
        return -1;
    source = (const struct facil_source *)
        (record + P32(record, 0x18) + 0x20);
    if (!FE_READ(source, source->value, 0x10004, &section, &size))
        return -1;
    if (facil_row_names(row, found) != 0)
        return -1;
    if (section != NULL && size != 0) {
        NUM_FORMAT(section + 0x08, 0x15, (char *)row + 0x1f);
        NUM_FORMAT(section + 0x10, 0x14, (char *)row + 0x34);
        NUM_FORMAT(section + 0x18, 0x1004e, (char *)row + 0x49);
        HOST_STRCPY((char *)row + 0x14, (const char *)section + 0x38);
        P32(row, 0x5c) = 0;
        for (offset = 0; offset < size;
             offset = offset + P32(section + offset, 4) + 8) {
            uint8_t *entry = section + offset;

            switch (P32(entry, 0x28)) {
            case 1:
                P32(row, 0x5c) |= 1;
                if (facil_bcd3((char *)row + 0x51, P32(entry, 0x24), 2) != 0)
                    return -1;
                break;
            case 2:
                P32(row, 0x5c) |= 4;
                if (facil_bcd3((char *)row + 0x51, P32(entry, 0x24), 2) != 0)
                    return -1;
                break;
            case 3:
                P32(row, 0x5c) |= 2;
                if (facil_bcd4((char *)row + 0x51, P32(entry, 0x24), 1) != 0)
                    return -1;
                break;
            case 8:
                P32(row, 0x5c) |= 0xc;
                break;
            }
        }
    }
    if (HOST_FREE(&record) != 0)
        return -1;
    if (section != NULL && HOST_FREE(&section) != 0)
        return -1;
    return 0;
}

/* .text+0x1d40 -- airport rows by city, case-sensitively; 0 if either has
 * none. */
static int32_t FS98_STDCALL facil_airport_compare(const void *left,
                                                  const void *right)
{
    const char *a = (const char *)PTR(P32(left, 0xc));
    const char *b;

    if (a != NULL) {
        b = (const char *)PTR(P32(right, 0xc));
        if (b != NULL)
            return FS98_HOST_STRCMP(HOST)(a, b);
    }
    return 0;
}

/* .text+0x1c90 -- navaid rows: those without bit 1 first, then by country,
 * state and name, case-insensitively. */
static int32_t FS98_STDCALL facil_navaid_compare(const void *left,
                                                 const void *right)
{
    const uint8_t *a = (const uint8_t *)left;
    const uint8_t *b = (const uint8_t *)right;
    int32_t result;

    if (P32(a, 0x5c) & 2) {
        if (!(b[0x5c] & 2))
            return 1;
    } else if (b[0x5c] & 2) {
        return -1;
    }
    if (P32(a, 4) != 0 && P32(b, 4) != 0) {
        result = FS98_HOST_STRICMP(HOST)((const char *)PTR(P32(a, 4)),
                                         (const char *)PTR(P32(b, 4)));
        if (result != 0)
            return result;
    }
    if (P32(a, 8) != 0 && P32(b, 8) != 0) {
        result = FS98_HOST_STRICMP(HOST)((const char *)PTR(P32(a, 8)),
                                         (const char *)PTR(P32(b, 8)));
        if (result != 0)
            return result;
    }
    return FS98_HOST_STRICMP(HOST)((const char *)PTR(P32(a, 0x10)),
                                   (const char *)PTR(P32(b, 0x10)));
}

/* ---- dialog 0x1307: facilities ------------------------------------------ */

/* .text+0x10e0 -- block bit 0: airports in each selected place. */
static int32_t facil_list_airports(uint8_t *block)
{
    struct fs98_host_list rows = { 0, 0, 0x20, 0x64, 0, 0, NULL };
    struct facil_results found;
    uint32_t index;
    uint32_t record;

    for (index = 0; index < P32(block, 0x10); index++) {
        if (facil_search_at(P32(PTR(P32(block, 0x14)), index * 4),
                            P32(PTR(P32(block, 0x18)), index * 4),
                            P32(PTR(P32(block, 0x1c)), index * 4),
                            (struct facil_filter *)(D + 0xd8), &found) != 0)
            return -1;
        for (record = 0; record < found.count; record++) {
            void *row = FS98_HOST_LIST_ADD(HOST)(&rows);
            if (facil_airport_row((uint8_t *)row,
                                  &found.records[record]) != 0)
                return -1;
        }
        if (HOST_FREE(&found.records) != 0)
            return -1;
    }
    P32(block, 0x24) = (uint32_t)rows.count;
    P32(block, 0x28) = U32(rows.data);
    if (rows.count == 0)
        return HOST_FREE(block + 0x28) != 0 ? -1 : 0;
    FS98_HOST_QSORT(HOST)(rows.data, (uint32_t)rows.count, 0x64,
                          facil_airport_compare);
    return 0;
}

/* .text+0x18d0 -- block bit 1: the kinds at .data+0x108 and +0x10c (VOR and
 * NDB) in each selected place. */
static int32_t facil_list_navaids(uint8_t *block)
{
    struct fs98_host_list rows = { 0, 0, 0x20, 0x60, 0, 0, NULL };
    struct facil_results found;
    uint32_t index;
    uint32_t kind;
    uint32_t record;

    for (index = 0; index < P32(block, 0x10); index++) {
        for (kind = 0x108; kind < 0x110; kind += 4) {
            D32(0x110) = D32(kind);
            if (facil_search_at(P32(PTR(P32(block, 0x14)), index * 4),
                                P32(PTR(P32(block, 0x18)), index * 4),
                                P32(PTR(P32(block, 0x1c)), index * 4),
                                (struct facil_filter *)(D + 0x110),
                                &found) != 0)
                return -1;
            for (record = 0; record < found.count; record++) {
                void *row = FS98_HOST_LIST_ADD(HOST)(&rows);
                if (facil_navaid_row((uint8_t *)row,
                                     &found.records[record]) != 0)
                    return -1;
            }
            if (HOST_FREE(&found.records) != 0)
                return -1;
        }
    }
    P32(block, 0x2c) = (uint32_t)rows.count;
    P32(block, 0x30) = U32(rows.data);
    if (rows.count == 0)
        return HOST_FREE(block + 0x30) != 0 ? -1 : 0;
    FS98_HOST_QSORT(HOST)(rows.data, (uint32_t)rows.count, 0x60,
                          facil_navaid_compare);
    return 0;
}

/* .text+0x1090 -- the block's two unused callbacks. */
static uint32_t FS98_STDCALL facil_callback_nothing(void *block)
{
    (void)block;
    return 0;
}

/* .text+0x10a0 -- the facilities block's apply. */
static uint32_t FS98_STDCALL facil_facilities_apply(uint8_t *block)
{
    if ((block[0x20] & 1) && facil_list_airports(block) != 0)
        return 0xffffffffu;
    if ((block[0x20] & 2) && facil_list_navaids(block) != 0)
        return 0xffffffffu;
    return 0;
}

/* .text+0x1040 -- Linkage +0x30, after resetting .data+0x80 (.text+0x1070). */
static uint32_t FS98_STDCALL facil_facilities_dialog(void)
{
    D32(0x80) = 0;
    D32(0xa8) = 0;
    D32(0xb0) = 0;
    D32(0xa4) = 0;
    D32(0xac) = 0;
    return FS98_HOST_DIALOG(HOST)(0x1307, D + 0x80) != 0 ? 0xffffffffu : 0;
}

/* .text+0x2840 -- the other dialogs' facilities button. */
static uint32_t FS98_STDCALL facil_facilities_button(void *block)
{
    (void)block;
    facil_facilities_dialog();
    return 0;
}

/* ---- dialog 0x1308: exact position --------------------------------------- */

/* .text+0x2450 */
static void facil_position_flags(void)
{
    G32(0xda8) = (int32_t)G32(0x564) > 0 ? 2 : 3;
    G32(0xdac) = G32(0x56c) >= 0x7fffffffu ? 1 : 0;
}

/* .text+0x24a0 -- position, heading and the indicated speed into the block's
 * variables, then into its controls through UTIL +0x90. */
static int32_t facil_position_load(void)
{
    D16(0x564) = (uint16_t)((uint16_t)(G32(0x580) >> 16) + 0x5b);
    if (FS98_HOST_MEMMOVE(HOST)(D + 0x568, GLOBAL + 0x560, 0x24) != 0)
        return -1;
    if (FS98_HOST_MEMMOVE(HOST)(D + 0x590, GLOBAL + 0xd50, 0x24) != 0)
        return -1;
    D32(0x560) = (G16(0x612) == 0 ? G32(0x2b8) : G32(0x2bc)) >> 7;
    UTIL_GET(D + 0x230, 8, D + 0x158);
    return 0;
}

/* .text+0x1e30 -- Linkage +0x28, after resetting the block (.text+0x1e60). */
static uint32_t FS98_STDCALL facil_position_dialog(void)
{
    D32(0x158) = 0;
    D32(0x174) = G16(0xc18);
    if (facil_position_load() != 0)
        return 0xffffffffu;
    NUM_FORMAT(D + 0x118, 0x10050, (char *)D + 0x5c0);
    NUM_FORMAT(D + 0x120, 0x10050, (char *)D + 0x5c8);
    NUM_FORMAT(D + 0x128, 0x1001b, (char *)D + 0x6c0);
    NUM_FORMAT(D + 0x12c, 0x1001b, (char *)D + 0x6b0);
    D32(0x1d0) = 0;
    D32(0x1e8) = 0;
    D32(0x200) = 0;
    D32(0x18c) = 0;
    D32(0x1a4) = 0;
    D32(0x5b4) = 0;
    return FS98_HOST_DIALOG(HOST)(0x1308, D + 0x158) != 0 ? 0xffffffffu : 0;
}

/* .text+0x2310 -- validate the edits; only the last parse decides. */
static uint32_t FS98_STDCALL facil_position_validate(uint8_t *block)
{
    uint8_t scratch[8];
    uint32_t result = 0;

    if (UTIL_PUT(D + 0x230, 8, D + 0x158, 1) != 0)
        result = 0xffffffffu;
    P32(block, 0x94) = (uint32_t)NUM_PARSE((const char *)PTR(P32(block, 0x98)),
                                           0x50, scratch, D + 0x118);
    P32(block, 0x50) = (uint32_t)NUM_PARSE((const char *)PTR(P32(block, 0x54)),
                                           0x50, scratch, D + 0x118);
    P32(block, 0xac) = (uint32_t)NUM_PARSE((const char *)PTR(P32(block, 0xb0)),
                                           0, scratch, D + 0x130);
    P32(block, 0xc0) = (uint32_t)NUM_PARSE((const char *)PTR(P32(block, 0xc4)),
                                           0x1b, scratch, D + 0x128);
    if (P32(block, 0xc0) != 0)
        result = 0xffffffffu;
    return result;
}

/* .text+0x1f20 -- write the changed fields back and move the aircraft. */
static uint32_t FS98_STDCALL facil_position_apply(uint8_t *block)
{
    if (UTIL_PUT(D + 0x258, 3, D + 0x158, 0) != 0)
        return 0xffffffffu;
    if (P32(block, 0x34) != 0) {
        if (UTIL_PUT(D + 0x230, 1, D + 0x158, 0) != 0 ||
            FS98_HOST_MEMMOVE(HOST)(GLOBAL + 0xd50, D + 0x590, 8) != 0)
            return 0xffffffffu;
    }
    if (P32(block, 0x4c) != 0) {
        if (UTIL_PUT(D + 0x244, 1, D + 0x158, 0) != 0 ||
            FS98_HOST_MEMMOVE(HOST)(GLOBAL + 0xd58, D + 0x598, 8) != 0)
            return 0xffffffffu;
    }
    if (D32(0x5b4) == 1) {
        if (UTIL_PUT(D + 0x230, 1, D + 0x158, 0) != 0 ||
            UTIL_PUT(D + 0x244, 1, D + 0x158, 0) != 0 ||
            FS98_HOST_MEMMOVE(HOST)(GLOBAL + 0xd50, D + 0x590, 8) != 0 ||
            FS98_HOST_MEMMOVE(HOST)(GLOBAL + 0xd58, D + 0x598, 8) != 0)
            return 0xffffffffu;
    }
    if (FS98_HOST_MEMMOVE(HOST)(GLOBAL + 0xd60, D + 0x5a0, 8) != 0)
        return 0xffffffffu;
    if (P32(block, 0x78) != 0) {
        if (UTIL_PUT(D + 0x294, 1, D + 0x158, 0) != 0 ||
            FS98_HOST_MEMMOVE(HOST)(GLOBAL + 0x560, D + 0x568, 8) != 0)
            return 0xffffffffu;
    }
    if (P32(block, 0x90) != 0) {
        if (UTIL_PUT(D + 0x2a8, 1, D + 0x158, 0) != 0 ||
            FS98_HOST_MEMMOVE(HOST)(GLOBAL + 0x568, D + 0x570, 8) != 0)
            return 0xffffffffu;
    }
    if (P32(block, 0xa8) != 0) {
        if (UTIL_PUT(D + 0x2bc, 1, D + 0x158, 0) != 0 ||
            FS98_HOST_MEMMOVE(HOST)(GLOBAL + 0x570, D + 0x578, 8) != 0)
            return 0xffffffffu;
        if (G32(0x574) != 0 || G16(0x572) != 0) {
            G8(0x366) = 0;
            if (P32(block, 0x78) != 0 || P32(block, 0x90) != 0 ||
                (int32_t)G32(0x574) >= ((int32_t)G32(0x20) >> 8))
                goto moved;
        }
        G8(0x366) = 1;
    }
moved:
    G32(0x580) = (D32(0x564) & 0xffff) << 16;
    facil_position_flags();
    SIM_34(D32(0x560));
    G16(0xcb8) = 1;
    CHAIN_DISPATCH(0x14, 0x33, 0);
    G8(0x365) = 7;
    G32(0x11f0) = 1;
    G32(0x11fc) = G32(0x574);
    G16(0x11fa) = G16(0x572);
    G32(0x11f4) = 1;
    G32(0x1200) = G32(0x580) - ((uint32_t)G16(0x2a0) << 16);
    MAIN_1C();
    MAIN_B0();
    return 0;
}

/* .text+0x23d0 -- put the original texts back. */
static uint32_t FS98_STDCALL facil_position_revert(uint8_t *block)
{
    HOST_STRCPY((char *)PTR(P32(block, 0x24)),
                (const char *)PTR(P32(block, 0x68)));
    HOST_STRCPY((char *)PTR(P32(block, 0x3c)),
                (const char *)PTR(P32(block, 0x80)));
    HOST_STRCPY((char *)PTR(P32(block, 0x54)),
                (const char *)PTR(P32(block, 0x98)));
    D32(0x5b4) = 1;
    return 0;
}

/* .text+0x2430 -- reload from the sim. */
static uint32_t FS98_STDCALL facil_position_refresh(void *block)
{
    (void)block;
    MAIN_C4();
    return facil_position_load() != 0 ? 0xffffffffu : 0;
}

/* ---- dialog 0x1309: airports --------------------------------------------- */

/* .text+0x2b00 -- free each record's name and the array; the selection
 * stays. */
static int32_t facil_free_names(uint8_t *list)
{
    while (P32(list, 0) != 0) {
        P32(list, 0)--;
        if (HOST_FREE(&RECORD(list, P32(list, 0))->name) != 0)
            return -1;
    }
    return HOST_FREE(list + 4) != 0 ? -1 : 0;
}

/* .text+0x2ae0 */
static int32_t facil_clear_list(uint8_t *list)
{
    P32(list, 8) = 0;
    return facil_free_names(list) != 0 ? -1 : 0;
}

/* .text+0x2e40 -- also free each record's payload, last first. */
static int32_t facil_free_payload_list(uint8_t *list)
{
    uint32_t count = P32(list, 0);

    while (count != 0) {
        count--;
        if (HOST_FREE(&RECORD(list, count)->node) != 0)
            return -1;
    }
    return facil_clear_list(list) != 0 ? -1 : 0;
}

/* .text+0x2a70 -- the airport list, whose payloads exist only in mode
 * 0x1002. */
static int32_t facil_free_result_list(uint8_t *list, uint32_t payloads)
{
    if (payloads != 0)
        return facil_free_payload_list(list);
    return facil_clear_list(list) != 0 ? -1 : 0;
}

/* .text+0x2ea0 -- the runway list: name and payload together, last first. */
static int32_t facil_free_runway_list(uint8_t *list)
{
    while (P32(list, 0) != 0) {
        struct facil_result *record;

        P32(list, 0)--;
        record = RECORD(list, P32(list, 0));
        if (HOST_FREE(&record->name) != 0 || HOST_FREE(&record->node) != 0)
            return -1;
    }
    if (HOST_FREE(list + 4) != 0)
        return -1;
    P32(list, 8) = 0;
    return 0;
}

/* .text+0x2b50 -- refill a list from +0x18, dropping a leading empty name
 * (whose string leaks). */
static int32_t facil_requery(uint8_t *list, struct facil_filter_list *filters,
                             struct facil_filter *filter, uint32_t dedupe)
{
    struct facil_results found;

    if (facil_free_names(list) != 0)
        return -1;
    if (facil_search(filters, filter, &found, dedupe) != 0)
        return -1;
    if (found.count != 0 && found.records[0].name[0] == 0) {
        found.count--;
        if (FS98_HOST_MEMMOVE(HOST)(found.records, found.records + 1,
                                    found.count * 0xc) != 0)
            return -1;
    }
    P32(list, 0) = found.count;
    P32(list, 4) = U32(found.records);
    return 0;
}

/* .text+0x2cd0 -- requery one place level without its own name, then select
 * the last record whose name equals it exactly (selection is index + 1). */
static int32_t facil_place_list(uint8_t *list, struct facil_filter_list *filters,
                                struct facil_filter *filter, char **name)
{
    char *saved = *name;
    int32_t result;

    *name = NULL;
    result = facil_requery(list, filters, filter, 1);
    *name = saved;
    P32(list, 8) = P32(list, 0);
    while ((int32_t)P32(list, 8) > 0) {
        P32(list, 8)--;
        if (FS98_HOST_STRCMP(HOST)(RECORD(list, P32(list, 8))->name,
                                   *name) == 0) {
            P32(list, 8)++;
            break;
        }
    }
    return result;
}

/* .text+0x2bf0 -- region, country, state and city, each filtered by the
 * other three of .data+0x2f8's names. */
static int32_t facil_place_lists(uint8_t *block)
{
    struct facil_filter filter;
    char **names = (char **)PTR(D32(0x2fc));

    filter.name = NULL;
    filter.kind = 0x10001;
    if (facil_place_list(block + 0x44, LIST_2F8, &filter,
                         (char **)((uint8_t *)names + 4)) != 0)
        return -1;
    filter.kind = 0x20001;
    if (facil_place_list(block + 0x50, LIST_2F8, &filter,
                         (char **)((uint8_t *)names + 0xc)) != 0)
        return -1;
    filter.kind = 0x30001;
    if (facil_place_list(block + 0x5c, LIST_2F8, &filter,
                         (char **)((uint8_t *)names + 0x14)) != 0)
        return -1;
    filter.kind = 0x40001;
    return facil_place_list(block + 0x68, LIST_2F8, &filter,
                            (char **)((uint8_t *)names + 0x1c)) != 0 ? -1 : 0;
}

/* .text+0x2f80 */
static int32_t facil_save_place(uint8_t *list, char *destination)
{
    if (P32(list, 8) == 0)
        HOST_STRCPY(destination, (const char *)D + 0xb18);
    else
        HOST_STRCPY(destination, RECORD(list, P32(list, 8) - 1)->name);
    return 0;
}

/* .text+0x2f10 -- the selected names into .data+0x978.., except on the first
 * call after opening. */
static int32_t facil_save_places(uint8_t *block)
{
    if (D32(0x2d0) != 0) {
        D32(0x2d0) = 0;
        return 0;
    }
    if (facil_save_place(block + 0x44, (char *)D + 0x978) == 0 &&
        facil_save_place(block + 0x50, (char *)D + 0x9e0) == 0 &&
        facil_save_place(block + 0x5c, (char *)D + 0xa48) == 0)
        facil_save_place(block + 0x68, (char *)D + 0xab0);
    return 0;
}

/* .text+0x2fd0 -- mode 0x1001: the airport of the selected name record.
 * The index is +0x9c itself, not a selection. */
static int32_t facil_airport_by_name(uint8_t *block)
{
    struct facil_filter filter = { 0x50001, NULL };
    struct facil_filter_list list = { 1, &filter };

    if (facil_free_names(block + 0x74) != 0)
        return -1;
    if (P32(block, 0x98) != 0 &&
        facil_describe(RECORD(block + 0x94, P32(block, 0x9c))->node, &list,
                       (struct facil_results *)(block + 0x74)) != 0)
        return -1;
    return 0;
}

/* .text+0x3590 -- one 0x10-byte area: its name and three words, through
 * UTIL's marshalling table at .data+0x300. */
static int32_t facil_unpack_area(uint8_t *area, const void *item)
{
    UTIL_MARSHAL(item, D + 0x300, 4);
    P32(area, 4) = D32(0x83c);
    P32(area, 8) = D32(0x840);
    P32(area, 0xc) = D32(0x844);
    P32(area, 0) = HOST_STRDUP((const char *)D + 0x6e0);
    return 0;
}

/* .text+0x3720 -- one 0x1c-byte navaid through the table at .data+0x340. */
static int32_t facil_unpack_navaid(uint8_t *navaid, const void *item)
{
    UTIL_MARSHAL(item, D + 0x340, 0xa);
    FS98_HOST_MEMMOVE(HOST)(navaid, D + 0x818, 0x1c);
    P32(navaid, 0) = HOST_STRDUP((const char *)D + 0x848);
    return 0;
}

/* .text+0x3480 -- FE's area listing into +0xac. */
static int32_t facil_list_areas(uint8_t *block)
{
    struct fs98_host_list list = { 0, 0, 0x20, 0xc, 0, 0, NULL };
    void *item = NULL;
    void *state = NULL;
    struct facil_result *record;

    if (P32(block, 0xb0) != 0)
        facil_free_payload_list(block + 0xac);
    P32(block, 0xac) = 0;
    P32(block, 0xb0) = 0;
    FE_AREA_FIRST();
    while (FE_AREA_NEXT(&item, &state)) {
        record = (struct facil_result *)FS98_HOST_LIST_ADD(HOST)(&list);
        if (FS98_HOST_ALLOC(HOST)(5, 0x10, (void **)&record->node, 0) != 0 ||
            facil_unpack_area(PTR(record->node), item) != 0)
            return -1;
        record->kind = 0;
        record->name = (char *)PTR(P32(PTR(record->node), 0));
    }
    P32(block, 0xac) = (uint32_t)list.count;
    P32(block, 0xb0) = U32(list.data);
    P32(block, 0xb4) = 0;
    return 0;
}

/* .text+0x35e0 -- mode 0x1002: FE's navaids around the selected area. */
static int32_t facil_list_navaids_at(uint8_t *block)
{
    struct fs98_host_list list = { 0, 0, 0x20, 0xc, 0, 0, NULL };
    void *item = NULL;
    void *state = NULL;
    struct facil_result *record;
    uint8_t *area;

    if (P32(block, 0x78) != 0 &&
        facil_free_result_list(block + 0x74, P32(block, 0xbc)) != 0)
        return -1;
    P32(block, 0x74) = 0;
    P32(block, 0x78) = 0;
    if (P32(block, 0xac) == 0)
        return 0;
    area = PTR(RECORD(block + 0xac, P32(block, 0xb4))->node);
    FE_NAV_FIRST(P32(area, 0xc), P32(area, 4), P32(area, 8));
    while (FE_NAV_NEXT(&item, &state)) {
        record = (struct facil_result *)FS98_HOST_LIST_ADD(HOST)(&list);
        if (FS98_HOST_ALLOC(HOST)(5, 0x1c, (void **)&record->node, 0) != 0 ||
            facil_unpack_navaid(PTR(record->node), item) != 0)
            return -1;
        record->kind = 0;
        record->name = (char *)PTR(P32(PTR(record->node), 0));
    }
    P32(block, 0x74) = (uint32_t)list.count;
    P32(block, 0x78) = U32(list.data);
    P32(block, 0x7c) = 0;
    return 0;
}

/* .text+0x3200 -- the selected airport's runways, one per type-1 record of
 * each source's 0x10005 section, named by number and L/R/C. */
static int32_t facil_list_runways(uint8_t *block)
{
    struct fs98_host_list list = { 0, 0, 0x20, 0xc, 0, 0, NULL };
    uint8_t *record;
    uint32_t record_size;
    uint8_t *section;
    uint32_t size;
    uint32_t *runway;
    char text[0x64];
    const struct facil_source *source;
    uint32_t index;
    uint32_t offset;

    if (facil_copy_record(RECORD(block + 0x74, P32(block, 0x7c))->node,
                          &record, &record_size) != 0)
        return -1;
    source = (const struct facil_source *)
        (record + P32(record, 0x18) + 0x20);
    for (index = 0; index < P32(record, 0x1c); index++, source++) {
        if (!FE_READ(source, source->value, 0x10005, &section, &size))
            return -1;
        for (offset = 0; offset < size;
             offset = offset + P32(section + offset, 4) + 8) {
            uint8_t *entry = section + offset;
            struct facil_result *row;

            if (P32(entry, 0x24) != 1)
                continue;
            row = (struct facil_result *)FS98_HOST_LIST_ADD(HOST)(&list);
            NUM_FORMAT(entry + 0x2c, 0x1001b, text);
            switch (P32(entry, 0x28)) {
            case 1: FS98_HOST_STRCAT(HOST)(text, (const char *)D + 0x4e0); break;
            case 2: FS98_HOST_STRCAT(HOST)(text, (const char *)D + 0x4e4); break;
            case 3: FS98_HOST_STRCAT(HOST)(text, (const char *)D + 0x4e8); break;
            }
            row->name = (char *)PTR(HOST_STRDUP(text));
            row->kind = 1;
            if (FS98_HOST_ALLOC(HOST)(5, 0x24, (void **)&runway, 1) != 0)
                return -1;
            runway[0] = P32(entry, 0x08);
            runway[1] = P32(entry, 0x0c);
            runway[2] = P32(entry, 0x10);
            runway[3] = P32(entry, 0x14);
            runway[4] = P32(entry, 0x18);
            runway[5] = P32(entry, 0x1c);
            runway[8] = P32(entry, 0x20);
            row->node = U32(runway);
        }
        if (HOST_FREE(&section) != 0)
            return -1;
    }
    P32(block, 0xa0) = (uint32_t)list.count;
    P32(block, 0xa8) = 0;
    P32(block, 0xa4) = U32(list.data);
    if (list.count == 0 && HOST_FREE(block + 0xa4) != 0)
        return -1;
    HOST_FREE(&record);
    return 0;
}

/* .text+0x3860 -- the chosen place names into the caller's output.  The
 * names are +0x1c's, handed over; the array at .data+0x7e8 leaks. */
static int32_t facil_fill_output(uint32_t node)
{
    uint32_t index;

    if (facil_describe(node, LIST_4D8,
                       (struct facil_results *)(D + 0x7e8)) != 0)
        return -1;
    for (index = 0; index < D32(0x7e8); index++) {
        struct facil_result *place = RECORD(D + 0x7e8, index);
        uint32_t name = U32(place->name);

        switch (place->kind) {
        case 0x10001: P32(CALLER_OUT, 0x00) = name; break;
        case 0x20001: P32(CALLER_OUT, 0x04) = name; break;
        case 0x30001: P32(CALLER_OUT, 0x08) = name; break;
        case 0x40001: P32(CALLER_OUT, 0x0c) = name; break;
        case 0x50001: P32(CALLER_OUT, 0x10) = name; break;
        case 0x50002: P32(CALLER_OUT, 0x14) = name; break;
        }
    }
    return 0;
}

/* .text+0x3120 -- the start position: the selected runway's threshold less
 * FE's ground height, or the aircraft's own position with no runways. */
static int32_t facil_airport_position(uint8_t *block, uint8_t *position)
{
    if (P32(block, 0xa0) != 0) {
        uint32_t node;

        if (FS98_HOST_MEMMOVE(HOST)(position,
                PTR(RECORD(block + 0xa0, P32(block, 0xa8))->node), 0x24) != 0)
            return -1;
        P32(position, 0x20) -= (uint32_t)FE_GROUND(position) << 16;
        if (D32(0x6d8) == 0)
            return 0;
        switch (P32(block, 0x28)) {
        case 0x1000: node = RECORD(block + 0x74, P32(block, 0x7c))->node; break;
        case 0x1001: node = RECORD(block + 0x94, P32(block, 0x9c))->node; break;
        default: return 0;
        }
        return facil_fill_output(node) != 0 ? -1 : 0;
    }
    return FS98_HOST_MEMMOVE(HOST)(position, GLOBAL + 0x55c, 0x24) != 0 ?
           -1 : 0;
}

/* .text+0x3a30 -- a double into a 48-bit longitude, wrapping negatives
 * round 2^48 (.data+0x3e0). */
static void facil_store_longitude(const double *value, uint8_t *out)
{
    long double v = *value;
    int64_t whole;
    uint32_t low;

    if (v < 0.0L)
        v = ((long double)*(double *)(D + 0x3e0) - v * -65536.0L) *
            (1.0L / 65536.0L);
    whole = facil_ftol(v);
    low = (uint32_t)whole;
    P32(out, 4) = low;
    P16(out, 2) = (uint16_t)facil_ftol((v - (long double)(int32_t)low) *
                                       65536.0L);
}

/* .text+0x3910 -- the tower position: 2,000 units out along heading + 0x18e2
 * from `position`, written to GLOBAL +0xd50.. */
static void facil_tower_from(uint8_t *position)
{
    int16_t sine;
    int16_t cosine;
    int32_t offset;
    uint8_t delta[8];
    long double sum;
    double longitude;

    G32(0xd54) = P32(position, 4);
    G32(0xd5c) = P32(position, 0xc);
    G32(0xd64) = P32(position, 0x14) + 0x1e;
    UTIL_SINCOS((P32(position, 0x20) >> 16) + 0x18e2, &sine, &cosine);
    G32(0xd54) += (uint32_t)(((int32_t)cosine * 2000) >> 16);
    offset = (int32_t)sine * 2000;
    UTIL_M32_TO_LON(&offset, position, delta);
    sum = ((long double)G32(0xd5c) + (long double)G16(0xd5a) * (1.0L / 65536.0L)) +
          ((long double)P16(delta, 2) * (1.0L / 65536.0L) +
           (long double)P32(delta, 4));
    longitude = (double)sum;
    facil_store_longitude(&longitude, GLOBAL + 0xd58);
}

/* .text+0x3a90 -- the tower from the first 0x10003 section of the selected
 * airport.  0 if found, -1 if not.  The section pointer starts out as the
 * block itself, so an airport with no sources copies the block into GLOBAL
 * and frees it. */
static int32_t facil_airport_tower(uint8_t *block)
{
    uint8_t *record;
    uint32_t record_size;
    uint32_t size;
    uint8_t *section = block;
    const struct facil_source *source;
    uint32_t index;
    int32_t result = 0;

    if (facil_copy_record(RECORD(block + 0x74, P32(block, 0x7c))->node,
                          &record, &record_size) != 0)
        return -1;
    source = (const struct facil_source *)
        (record + P32(record, 0x18) + 0x20);
    for (index = 0; index < P32(record, 0x1c); index++, source++) {
        if (!FE_READ(source, source->value, 0x10003, &section, &size))
            return -1;
        if (section != NULL)
            break;
    }
    if (section != NULL) {
        G32(0xd50) = P32(section, 0x08);
        G32(0xd54) = P32(section, 0x0c);
        G32(0xd58) = P32(section, 0x10);
        G32(0xd5c) = P32(section, 0x14);
        G32(0xd60) = P32(section, 0x18);
        G32(0xd64) = P32(section, 0x1c);
    } else {
        result = -1;
    }
    if (HOST_FREE(&section) != 0 || HOST_FREE(&record) != 0)
        return -1;
    return result;
}

/* .text+0x3770 -- a navaid as a start position. */
static int32_t facil_navaid_position(const uint8_t *navaid, uint8_t *position)
{
    uint32_t packed = P32(navaid, 0xc);
    double altitude;

    P32(position, 4) = (uint32_t)((int32_t)P32(navaid, 4) >> 7);
    P16(position, 0xa) = 0;
    P16(position, 2) = (uint16_t)((P16(navaid, 4) & 0xff80) << 1);
    P32(position, 0xc) = P32(navaid, 8);
    altitude = (double)((long double)(packed >> 8) -
                        (long double)(packed & 0xff) * (-1.0L / 256.0L));
    UTIL_FROM_DOUBLE(&altitude, position + 0x10);
    P32(position, 0x18) = 0;
    P32(position, 0x1c) = 0;
    P32(position, 0x20) = (uint32_t)P16(navaid, 0x10) << 16;
    return 0;
}

/* .text+0x3050 -- put the aircraft there. */
static int32_t facil_move_aircraft(const uint8_t *position)
{
    FS98_HOST_MEMMOVE(HOST)(GLOBAL + 0x560, position, 0x18);
    G32(0x580) = P32(position, 0x20);
    SIM_1C();
    facil_position_flags();
    G16(0xcb8) = 1;
    CHAIN_DISPATCH(0x14, 0x33, 0);
    G8(0x365) = 7;
    G32(0x11f0) = 1;
    G32(0x11fc) = G32(0x574);
    G16(0x11fa) = G16(0x572);
    G32(0x11f4) = 1;
    G32(0x1200) = G32(0x580);
    MAIN_B0();
    return 0;
}

/* .text+0x2d60 -- free every list and clear the name filter. */
static int32_t facil_airport_clear(uint8_t *block)
{
    if (facil_clear_list(block + 0x44) != 0 ||
        facil_clear_list(block + 0x50) != 0 ||
        facil_clear_list(block + 0x5c) != 0 ||
        facil_clear_list(block + 0x68) != 0 ||
        facil_free_result_list(block + 0x74, P32(block, 0xbc)) != 0 ||
        facil_clear_list(block + 0x94) != 0 ||
        facil_free_runway_list(block + 0xa0) != 0 ||
        facil_free_payload_list(block + 0xac) != 0)
        return -1;
    HOST_STRCPY((char *)PTR(P32(block, 0x84)), (const char *)D + 0xb18);
    return 0;
}

/* .text+0x26a0 -- OK: remember the mode and move to the selection.  The
 * position is an uninitialised stack buffer, as in retail: a navaid fills
 * only part of it, any other mode with a nonempty list none of it, and the
 * rest reaches GLOBAL +0x560. */
static uint32_t FS98_STDCALL facil_airport_accept(uint8_t *block)
{
    uint8_t position[0x24];
    uint32_t mode;

    D32(0x4ec) = P32(block, 0x28);
    if (P32(block, 0x74) == 0)
        return 0;
    mode = P32(block, 0x28);
    if (mode == 0x1002) {
        if (facil_navaid_position(
                PTR(RECORD(block + 0x74, P32(block, 0x7c))->node),
                position) != 0)
            return 0xffffffffu;
        facil_tower_from(position);
        if (D32(0x6d8) != 0) {
            P32(CALLER_OUT, 0) = HOST_STRDUP(
                RECORD(block + 0xac, P32(block, 0xb4))->name);
            P32(CALLER_OUT, 0x10) = HOST_STRDUP(
                RECORD(block + 0x74, P32(block, 0x7c))->name);
        }
    } else if ((int32_t)mode >= 0x1000 && (int32_t)mode <= 0x1001) {
        if (facil_airport_position(block, position) != 0)
            return 0xffffffffu;
        if (facil_airport_tower(block) != 0)
            facil_tower_from(position);
    }
    if (facil_move_aircraft(position) != 0)
        return 0xffffffffu;
    HOST_STRCPY((char *)D + 0x2b60, (const char *)D + 0x978);
    HOST_STRCPY((char *)D + 0x2bc8, (const char *)D + 0x9e0);
    HOST_STRCPY((char *)D + 0x2c30, (const char *)D + 0xa48);
    HOST_STRCPY((char *)D + 0x2c98, (const char *)D + 0xab0);
    HOST_STRCPY((char *)D + 0xb20, (const char *)D + 0x1300);
    HOST_STRCPY((char *)D + 0xf10, (const char *)D + 0x16f0);
    return 0;
}

/* .text+0x28a0 -- the place lists. */
static uint32_t FS98_STDCALL facil_airport_places(uint8_t *block)
{
    if (facil_save_places(block) != 0)
        return 0xffffffffu;
    return facil_place_lists(block) != 0 ? 0xffffffffu : 0;
}

/* .text+0x28d0 -- the name list: name records (0x50002) starting with the
 * filter text, deduplicated. */
static uint32_t FS98_STDCALL facil_airport_names(uint8_t *block)
{
    struct facil_filter filter;

    filter.kind = 0x50002;
    filter.name = (const char *)PTR(P32(block, 0x84));
    if (facil_requery(block + 0x94, NULL, &filter, 1) != 0)
        return 0xffffffffu;
    P32(block, 0x9c) = 0;
    return 0;
}

/* .text+0x2920 -- the airport list for the mode.  Mode 0x1002 frees it a
 * second time, harmlessly. */
static uint32_t FS98_STDCALL facil_airport_airports(uint8_t *block)
{
    struct facil_filter filter;

    if (facil_free_result_list(block + 0x74, P32(block, 0xbc)) != 0)
        return 0xffffffffu;
    switch (P32(block, 0x28)) {
    case 0x1002:
        if (facil_free_result_list(block + 0x74, P32(block, 0xbc)) != 0 ||
            facil_list_navaids_at(block) != 0)
            return 0xffffffffu;
        P32(block, 0xbc) = 1;
        return 0;
    case 0x1001:
        if (facil_airport_by_name(block) != 0)
            return 0xffffffffu;
        P32(block, 0xbc) = 0;
        P32(block, 0x7c) = 0;
        return 0;
    case 0x1000:
        filter.kind = 0x50001;
        filter.name = NULL;
        if (facil_requery(block + 0x74, LIST_2F8, &filter, 0) != 0)
            return 0xffffffffu;
        P32(block, 0xbc) = 0;
        P32(block, 0x7c) = 0;
        return 0;
    }
    return 0;
}

/* .text+0x2a20 -- the runway list. */
static uint32_t FS98_STDCALL facil_airport_runways(uint8_t *block)
{
    if (facil_free_runway_list(block + 0xa0) != 0)
        return 0xffffffffu;
    if ((int32_t)P32(block, 0x28) >= 0x1000 &&
        (int32_t)P32(block, 0x28) <= 0x1001 && P32(block, 0x74) != 0 &&
        facil_list_runways(block) != 0)
        return 0xffffffffu;
    return 0;
}

/* .text+0x2850 -- everything. */
static uint32_t FS98_STDCALL facil_airport_refresh(uint8_t *block)
{
    if (facil_airport_clear(block) != 0 ||
        facil_airport_places(block) != 0 ||
        facil_airport_names(block) != 0)
        return 0xffffffffu;
    return facil_airport_airports(block) != 0 ? 0xffffffffu : 0;
}

/* .text+0x3810 -- the area changed. */
static uint32_t FS98_STDCALL facil_airport_area(uint8_t *block)
{
    if (facil_free_payload_list(block + 0xac) != 0 ||
        facil_free_result_list(block + 0x74, P32(block, 0xbc)) != 0)
        return 0xffffffffu;
    return facil_list_areas(block) != 0 ? 0xffffffffu : 0;
}

/* .text+0x2550 -- Linkage +0x24.  `out`, if given, receives six names: the
 * four places, the airport and its name record (only five are cleared).
 * Returns the block's answer, or 0 on any failure. */
static uint32_t FS98_STDCALL facil_airport_dialog(uint8_t *out)
{
    D32(0x6d8) = U32(out);
    D32(0x410) = D32(0x4ec);
    D32(0x3e8) = 0;
    D32(0x2d0) = 1;
    D32(0x414) = G16(0xc18);
    if (D32(0x6d8) != 0) {
        P32(CALLER_OUT, 0x0) = 0;
        P32(CALLER_OUT, 0x4) = 0;
        P32(CALLER_OUT, 0x8) = 0;
        P32(CALLER_OUT, 0xc) = 0;
        P32(CALLER_OUT, 0x10) = 0;
    }
    HOST_STRCPY((char *)D + 0x978, (const char *)D + 0x2b60);
    HOST_STRCPY((char *)D + 0x9e0, (const char *)D + 0x2bc8);
    HOST_STRCPY((char *)D + 0xa48, (const char *)D + 0x2c30);
    HOST_STRCPY((char *)D + 0xab0, (const char *)D + 0x2c98);
    HOST_STRCPY((char *)D + 0x1300, (const char *)D + 0x55c);
    HOST_STRCPY((char *)D + 0x16f0, (const char *)D + 0x55c);
    if (FS98_HOST_DIALOG(HOST)(0x1309, D + 0x3e8) != 0)
        return 0;
    return facil_airport_clear(D + 0x3e8) != 0 ? 0 : D32(0x3e8);
}

/* The .text targets of .data's relocations. */
static facil_fn facil_text_target(uint32_t rva)
{
    switch (rva) {
    case 0x1090: return (facil_fn)facil_callback_nothing;
    case 0x10a0: return (facil_fn)facil_facilities_apply;
    case 0x1f20: return (facil_fn)facil_position_apply;
    case 0x2310: return (facil_fn)facil_position_validate;
    case 0x23d0: return (facil_fn)facil_position_revert;
    case 0x2430: return (facil_fn)facil_position_refresh;
    case 0x2840: return (facil_fn)facil_facilities_button;
    case 0x26a0: return (facil_fn)facil_airport_accept;
    case 0x3810: return (facil_fn)facil_airport_area;
    case 0x2850: return (facil_fn)facil_airport_refresh;
    case 0x28a0: return (facil_fn)facil_airport_places;
    case 0x28d0: return (facil_fn)facil_airport_names;
    case 0x2920: return (facil_fn)facil_airport_airports;
    case 0x2a20: return (facil_fn)facil_airport_runways;
    }
    return (facil_fn)facil_unimplemented;
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
    D32(offset) = (uint32_t)(uintptr_t)facil_text_target(target);
#include "facil_data.inc"
#undef FACIL_REL_TEXT
#undef FACIL_REL_DATA
#undef FACIL_RELOCS

    facil_image.linkage.init = facil_init;
    facil_image.linkage.deinit = facil_deinit;
    facil_image.linkage.slots[0] = (facil_fn)facil_search;
    facil_image.linkage.slots[1] = (facil_fn)facil_describe;
    facil_image.linkage.slots[2] = (facil_fn)facil_copy_record;
    facil_image.linkage.slots[3] = (facil_fn)facil_airport_dialog;
    facil_image.linkage.slots[4] = (facil_fn)facil_position_dialog;
    facil_image.linkage.slots[5] = (facil_fn)facil_rebuild;
    facil_image.linkage.slots[6] = (facil_fn)facil_facilities_dialog;
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
