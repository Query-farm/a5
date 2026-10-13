# Upgrading to A5 1.0

This version of the extension is built on [A5 1.0](https://crates.io/crates/a5), a breaking release
of the underlying library. Results change in ways that matter if you have stored cell IDs or query
code written against an earlier version of the extension.

## What changed

- **Cell IDs changed.** A5 1.0 orders cells along a new curve, so the same cell has a different ID
  at resolutions 2 and finer. IDs stored by an earlier version are still well-formed — they silently
  refer to *different* cells — so migrate them once with `a5_migrate`:
  ```sql
  UPDATE my_table SET cell_id = a5_migrate(cell_id);
  ```
  `a5_migrate` must be applied exactly once, and only to pre-1.0 IDs; there is no way to tell from an
  ID which index it belongs to.
- **Resolution-1 cells were redefined.** Resolution-1 IDs are left unchanged by `a5_migrate`, but
  the cells they name have a different shape than before, so a point may now fall in a different
  resolution-1 cell. Re-index resolution-1 data from its source coordinates.
- **Coverings end in a compaction marker.** `a5_compact`, `a5_grid_disk`, `a5_grid_disk_vertex`,
  `a5_spherical_cap` and polygon results of `a5_geometry_to_cells` now return one extra element that
  is not a cell — see [Coverings](#coverings-and-compaction-markers) below. Code
  that unnests these lists or takes their `len()` should go through `a5_uncompact` / `a5_count`.
- **`a5_uncompact` no longer takes a resolution.** `a5_uncompact(cells, resolution)` is gone; a
  covering now carries its own resolution, so write `a5_uncompact(cells)`. For results of
  `a5_geometry_to_cells`, `a5_grid_disk`, `a5_spherical_cap` and `a5_compact` this is a drop-in
  change. To expand a hand-built list of cells to a chosen resolution, expand each cell instead:
  ```sql
  SELECT flatten(list_transform(cells, c -> a5_cell_to_children(c, 12)));
  ```
- **`a5_get_num_cells` is exact** at resolutions 28-30, where it previously returned rounded values.
- **Resolution `-1` is accepted** as the world cell by `a5_cell_to_parent`, `a5_cell_to_supercell`,
  `a5_get_num_children` and `a5_cell_area`, which previously raised an error, and
  `a5_cell_to_parent(cell)` without a resolution returns the immediate parent.

A quick checklist for existing queries:

| If you have… | Change it to… |
|---|---|
| stored cell IDs (resolution 2+) | `a5_migrate(cell_id)`, once |
| stored resolution-1 cell IDs | re-index from source coordinates |
| `a5_uncompact(x, res)` | `a5_uncompact(x)` |
| `unnest(a5_grid_disk(...))` and other coverings | `unnest(a5_uncompact(a5_grid_disk(...)))` |
| `len(...)` of a covering | `a5_count(...)` |
| `list_contains(covering, cell)` | `a5_contains(covering, cell)` |

## Coverings and compaction markers

A **covering** is how A5 represents a region as a list of cells: the cells are **compacted** (every
complete group of siblings is replaced by its parent, recursively), sorted in curve order, and
followed by one **compaction marker** — a `UBIGINT` that is *not* a cell and records the resolution
the cells stand for. Without it a compacted list could not be expanded again: a region that
compacts to a single resolution-5 cell would look the same whether it was built at resolution 6
or 12.

`a5_compact`, `a5_grid_disk`, `a5_grid_disk_vertex`, `a5_spherical_cap`, `a5_cell_to_subcell`,
`a5_union`, `a5_intersect`, `a5_difference` and polygon results of `a5_geometry_to_cells` all return
coverings. Every function taking a list of cells accepts a covering, and also a plain list of cells
without a marker, whose resolution is then that of its finest cell.

Because the marker is not a cell, expand a covering before treating its elements as cells
(`unnest(a5_uncompact(cells))`), or drop the marker with
`list_filter(cells, x -> NOT a5_is_compaction_marker(x))` if you want the compacted cells themselves.

```sql
SELECT a5_grid_disk(a5_lonlat_to_cell(-74.0060, 40.7128, 15), 1) as neighbors;
-- [2760557102958116864, 2760559170447998976, 2760559181722288128, 17298044693753364544]
--  three compacted cells, then the marker (resolution 15)

SELECT unnest(a5_uncompact(a5_grid_disk(a5_lonlat_to_cell(-74.0060, 40.7128, 15), 1))) as neighbor;
-- 6 rows: the cell and its five edge neighbors
```

## New functions

| Function | Returns | Description |
|----------|---------|-------------|
| `a5_migrate(cell)` | `UBIGINT` | Convert a pre-1.0 cell ID to the current index |
| `a5_is_compaction_marker(value)` | `BOOLEAN` | Whether a value is a compaction marker |
| `a5_covering_resolution(cells)` | `INTEGER` | Resolution of a set of cells |
| `a5_count(cells)` | `UBIGINT` | Number of cells in a set at its resolution |
| `a5_area(cells)` | `DOUBLE` | Area (m²) of a set of cells |
| `a5_union(a, b)` | `UBIGINT[]` | Cells in either set |
| `a5_intersect(a, b)` | `UBIGINT[]` | Cells in both sets |
| `a5_difference(a, b)` | `UBIGINT[]` | Cells in `a` but not `b` |
| `a5_overlaps(a, b)` | `BOOLEAN` | Whether two sets share any cell |
| `a5_contains(cells, cell)` | `BOOLEAN` | Whether a cell is in a set |
| `a5_cell_to_supercell(cell, res)` | `UBIGINT` | Coarser cell containing the cell's center |
| `a5_cell_to_subcell(cell, res)` | `UBIGINT[]` | Finer cells whose centers lie in the cell |
| `a5_max_resolution()` | `INTEGER` | The finest resolution (`30`) |

The set operations and `a5_contains` need both arguments at the same resolution and raise an error
otherwise. `a5_cell_to_supercell` / `a5_cell_to_subcell` are the spatial counterparts of
`a5_cell_to_parent` / `a5_cell_to_children`: A5 cells do not nest exactly across resolutions, so the
parent of a cell need not contain it, while its supercell always contains its center.

Every function carries its description, parameters and an example in DuckDB itself:

```sql
SELECT function_name, parameters, description, examples
FROM duckdb_functions() WHERE function_name LIKE 'a5_%' ORDER BY ALL;
```
