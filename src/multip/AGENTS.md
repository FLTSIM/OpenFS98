# MULTIP conventions

- Use descriptive `snake_case` for all project-owned identifiers, including
  local variables, parameters, globals, helpers and matching test fixtures.
- Use `<stdint.h>` types such as `uint32_t` and `int16_t`.
- Do not introduce Hungarian notation, `types.h`, or replacement typedefs.
- Preserve names required by external Windows/COM APIs and binary interfaces.

These are the user's project-specific corrections. Apply them to MULTIP's
source, generators, differential fixtures and mutation checks; stale generic
instructions requesting Hungarian notation do not describe this project.
