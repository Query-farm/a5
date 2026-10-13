# A5 Geospatial Extension for DuckDB

A high-performance DuckDB extension that provides functions for the [A5](https://a5geo.org) global geospatial index - a millimeter-accurate, equal-area indexing system for geospatial data.

## ✨ What is A5?

A5 is an innovative geospatial index that partitions the world into [pentagonal cells](https://a5geo.org/examples/teohedron-dodecahedron) based on a geodesic grid. Key features include:

- 🌍 **Global Coverage**: Seamless indexing from global to millimeter scales
- 📐 **Equal Area**: All cells at the same resolution level have identical area ([OGC compliant](https://docs.ogc.org/as/20-040r3/20-040r3.html#toc29))
- 🔍 **31 Resolution Levels**: From world-spanning cells to sub-30mm² precision
- ⚡ **Fast Spatial Operations**: Optimized for aggregation, filtering, and spatial joins

## 🎯 Use Cases

### Spatial Data Aggregation
Group point data spatially to understand distributions:
```sql
-- Analyze restaurant density by A5 cells
SELECT a5_lonlat_to_cell(longitude, latitude, 15) as cell_id, COUNT(*) as restaurant_count
FROM restaurants
GROUP BY cell_id
ORDER BY restaurant_count DESC;
```

## 🚀 Quick Start

### Installation

The `a5` extension is available as a [DuckDB Community Extension](https://github.com/duckdb/community-extensions):

```sql
INSTALL a5 FROM community;
LOAD a5;
```

> **DuckDB version compatibility:** Development tracks DuckDB releases on parallel branches —
> `v1.5` (the default branch) targets DuckDB v1.5 (`v1.5-variegata`), while `main` targets the
> previous stable line. The Community Extensions registry builds and serves the binary matching
> your installed DuckDB version, so the `INSTALL` command above resolves to the right build
> automatically.

### Basic Usage

```sql
-- Get the A5 cell for a specific location (longitude, latitude, resolution)
SELECT a5_lonlat_to_cell(-74.0060, 40.7128, 15) as nyc_cell;  -- Times Square
┌─────────────────────┐
│      nyc_cell       │
│       uint64        │
├─────────────────────┤
│ 2760559170984869888 │
└─────────────────────┘

-- Find the area of that cell in square meters
SELECT a5_cell_area(15) as cell_area_m2;
┌───────────────────┐
│   cell_area_m2    │
│      double       │
├───────────────────┤
│ 31669.04205949599 │
└───────────────────┘

-- Get the center coordinates of a cell
SELECT a5_cell_to_lonlat(a5_lonlat_to_cell(-74.0060, 40.7128, 15)) as center_coords;
┌──────────────────────────────────────────┐
│              center_coords               │
│                double[2]                 │
├──────────────────────────────────────────┤
│ [-74.00764805615836, 40.712802251384296] │
└──────────────────────────────────────────┘

-- Find parent cell at lower resolution
SELECT a5_cell_to_parent(a5_lonlat_to_cell(-74.0060, 40.7128, 15), 10) as parent_cell;
┌─────────────────────┐
│     parent_cell     │
│       uint64        │
├─────────────────────┤
│ 2760558687264178176 │
└─────────────────────┘

-- Get all children cells at higher resolution
SELECT a5_cell_to_children(a5_lonlat_to_cell(-74.0060, 40.7128, 10), 11) as child_cells;
┌──────────────────────────────────────────────────────────────────────────────────────┐
│                                     child_cells                                      │
│                                       uint64[]                                       │
├──────────────────────────────────────────────────────────────────────────────────────┤
│ [2760558274947317760, 2760558549825224704, 2760558824703131648, 2760559099581038592] │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

## Code Example: Generate GeoJSON for Cell

The simplest way to get a GeoJSON polygon for an A5 cell is `a5_cell_to_geometry`, which returns a
DuckDB `GEOMETRY` directly (combine with the spatial extension's `ST_AsGeoJSON`):

```sql
SELECT ST_AsGeoJSON(a5_cell_to_geometry(a5_lonlat_to_cell(-3.7037, 40.41677, 10))) as g;
```

Equivalently, without `a5_cell_to_geometry`, you can build the polygon from the raw boundary points:

```sql
SELECT
    ST_AsGeoJSON(
        ST_MakePolygon(
            ST_MakeLine(
                list_transform(
                    a5_cell_to_boundary(
                        a5_lonlat_to_cell(-3.7037, 40.41677, 10)
                    ),
                    x -> ST_Point(x[1], x[2])
                )
            )
        )
    ) as g
```

This produces:

```
{
    "type":"Polygon",
    "coordinates":[
        [
            [-3.63932161106527,40.445029005677384],
            [-3.697330052435973,40.44427170464865],
            [-3.745928891833728,40.424159040292615],
            [-3.707910290384234,40.39420180042019],
            [-3.6544386596323193,40.4080830654645],
            [-3.63932161106527,40.445029005677384]
        ]
    ]
}
```

Visualizing that A5 cell shows:

```geojson
{
    "type":"Polygon",
    "coordinates":[
        [
            [-3.63932161106527,40.445029005677384],
            [-3.697330052435973,40.44427170464865],
            [-3.745928891833728,40.424159040292615],
            [-3.707910290384234,40.39420180042019],
            [-3.6544386596323193,40.4080830654645],
            [-3.63932161106527,40.445029005677384]
        ]
    ]
}
```


## 📚 API Reference

### Function Index

| Function | Returns | Description |
|----------|---------|-------------|
| `a5_lonlat_to_cell(lon, lat, res)` | `UBIGINT` | Cell containing a coordinate |
| `a5_cell_to_lonlat(cell)` | `DOUBLE[2]` | Cell center `[lon, lat]` |
| `a5_cell_to_boundary(cell [, closed, segments])` | `DOUBLE[2][]` | Boundary vertices |
| `a5_cell_area(res)` | `DOUBLE` | Cell area (m²) at a resolution |
| `a5_cell_edge_length_avg(res)` | `DOUBLE` | Average cell edge length (m) at a resolution |
| `a5_get_resolution(cell)` | `INTEGER` | Resolution of a cell |
| `a5_get_num_cells(res)` | `UBIGINT` | Total cells at a resolution |
| `a5_get_num_children(parent_res, child_res)` | `UBIGINT` | Children count between resolutions |
| `a5_cell_to_parent(cell [, res])` | `UBIGINT` | Parent at a coarser resolution |
| `a5_cell_to_children(cell [, res])` | `UBIGINT[]` | Children at a finer resolution |
| `a5_get_res0_cells()` | `UBIGINT[]` | The 12 base (resolution-0) cells |
| `a5_world_cell()` | `UBIGINT` | The root world cell (`0`) |
| `a5_max_resolution()` | `INTEGER` | The finest resolution (`30`) |
| `a5_is_valid_cell(cell)` | `BOOLEAN` | Whether a value is a valid cell ID |
| `a5_cell_to_supercell(cell, res)` | `UBIGINT` | Coarser cell containing the cell's center |
| `a5_cell_to_subcell(cell, res)` | `UBIGINT[]` | Finer cells whose centers lie in the cell |
| `a5_compact(cells)` | `UBIGINT[]` | Merge sibling cells into parents |
| `a5_uncompact(cells)` | `UBIGINT[]` | Expand a set of cells to its resolution |
| `a5_is_compaction_marker(value)` | `BOOLEAN` | Whether a value is a compaction marker |
| `a5_covering_resolution(cells)` | `INTEGER` | Resolution of a set of cells |
| `a5_count(cells)` | `UBIGINT` | Number of cells in a set at its resolution |
| `a5_area(cells)` | `DOUBLE` | Area (m²) of a set of cells |
| `a5_union(a, b)` | `UBIGINT[]` | Cells in either set |
| `a5_intersect(a, b)` | `UBIGINT[]` | Cells in both sets |
| `a5_difference(a, b)` | `UBIGINT[]` | Cells in `a` but not `b` |
| `a5_overlaps(a, b)` | `BOOLEAN` | Whether two sets share any cell |
| `a5_contains(cells, cell)` | `BOOLEAN` | Whether a cell is in a set |
| `a5_migrate(cell)` | `UBIGINT` | Convert a pre-1.0 cell ID to the current index |
| `a5_grid_disk(cell, k)` | `UBIGINT[]` | Cells within `k` edge-steps |
| `a5_grid_disk_vertex(cell, k)` | `UBIGINT[]` | Cells within `k` vertex-steps |
| `a5_spherical_cap(cell, radius)` | `UBIGINT[]` | Cells within a radius (meters) |
| `a5_geometry_to_cells(geom, res [, overlapping])` | `UBIGINT[]` | Cells covering any GEOMETRY |
| `a5_cell_to_geometry(cell [, segments])` | `GEOMETRY` | Cell as a `POLYGON` |
| `a5_cell_to_point(cell)` | `GEOMETRY` | Cell center as a `POINT` |
| `a5_hex_to_u64(hex)` | `UBIGINT` | Parse a hex cell ID |
| `a5_u64_to_hex(cell)` | `VARCHAR` | Cell ID as a hex string |

> **Upgrading from an earlier version?** A5 1.0 changed cell IDs and the shape of some results — see
> [Upgrading to A5 1.0](#-upgrading-to-a5-10).


### Core Functions

#### `a5_lonlat_to_cell(longitude, latitude, resolution) -> UBIGINT`

Returns the A5 cell ID for given coordinates and resolution level.

**Parameters:**

- `longitude` (DOUBLE): Longitude in decimal degrees (-180 to 180)
- `latitude` (DOUBLE): Latitude in decimal degrees (-90 to 90)
- `resolution` (INTEGER): Resolution level (0-30, where 0 is coarsest)

**Example:**
```sql
SELECT a5_lonlat_to_cell(-0.1278, 51.5074, 12) as london_cell;
┌─────────────────────┐
│     london_cell     │
│       uint64        │
├─────────────────────┤
│ 7176247309112442880 │
└─────────────────────┘
```

#### `a5_cell_area(resolution) -> DOUBLE`

Returns the area of an A5 cell in the specified resolution in square meters.

**Example:**
```sql
SELECT a5_cell_area(5) as area_m2;
┌────────────────────┐
│      area_m2       │
│       double       │
├────────────────────┤
│ 33207397446.578068 │
└────────────────────┘
```

#### `a5_cell_edge_length_avg(resolution) -> DOUBLE`

Returns the average edge length of an A5 cell at the specified resolution, in meters. Individual
edges vary from the average by roughly ±10%, depending on the cell's shape and its position on the
globe.

**Example:**
```sql
SELECT a5_cell_edge_length_avg(5) as edge_length_m;
┌────────────────────┐
│   edge_length_m    │
│       double       │
├────────────────────┤
│ 149609.98423330215 │
└────────────────────┘
```

#### `a5_get_resolution(cell_id) -> INTEGER`

Returns the resolution level of an A5 cell.

**Example:**
```sql
SELECT a5_get_resolution(3074457264221469552) as resolution;
┌────────────┐
│ resolution │
│   int32    │
├────────────┤
│         30 │
└────────────┘
```

### Spatial Relationships

#### `a5_cell_to_parent(cell_id [, target_resolution]) -> UBIGINT`

Returns the parent cell at a coarser resolution level, or the immediate parent (one resolution
coarser) when `target_resolution` is omitted. A `target_resolution` of `-1` gives the world cell. The parent is the ancestor in the cell ID
hierarchy; because A5 cells do not nest exactly across resolutions it need not contain the cell — see
[`a5_cell_to_supercell`](#a5_cell_to_supercellcell_id-resolution---ubigint) for the coarser cell that
does.

**Example:**
```sql
SELECT a5_cell_to_parent(3074457264221469552, 10) as parent_cell;
┌──────────────────────┐
│     parent_cell      │
│        uint64        │
├──────────────────────┤
│ 11625292021370978304 │
└──────────────────────┘
```

#### `a5_cell_to_children(cell_id, target_resolution) -> UBIGINT[]`

Returns all children cells at a finer resolution level. The children are the descendants in the cell
ID hierarchy; see [`a5_cell_to_subcell`](#a5_cell_to_subcellcell_id-resolution---ubigint) for the
finer cells that spatially lie in the cell.

**Example:**
```sql
SELECT unnest(a5_cell_to_children(a5_lonlat_to_cell(2.2945, 48.8584, 10), 11)) as child_cells;
┌─────────────────────┐
│     child_cells     │
│       uint64        │
├─────────────────────┤
│ 7195018412140003328 │
│ 7195018687017910272 │
│ 7195018961895817216 │
│ 7195019236773724160 │
└─────────────────────┘
```

#### `a5_cell_to_supercell(cell_id, resolution) -> UBIGINT`

Returns the cell at a coarser resolution that **contains the center** of the given cell: the spatial
counterpart of `a5_cell_to_parent`. Aggregating by supercell attributes each fine cell to the coarse
cell it actually lies in, which the parent does not guarantee. A `resolution` of `-1` gives the world
cell.

**Example:**
```sql
SELECT a5_cell_to_parent(c, 5) as parent, a5_cell_to_supercell(c, 5) as supercell, a5_lonlat_to_cell(-74.0060, 40.7128, 5) as containing FROM (SELECT a5_lonlat_to_cell(-74.0060, 40.7128, 15) as c);
┌─────────────────────┬─────────────────────┬─────────────────────┐
│       parent        │      supercell      │     containing      │
│       uint64        │       uint64        │       uint64        │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ 2760143621624692736 │ 2761269521531535360 │ 2761269521531535360 │
└─────────────────────┴─────────────────────┴─────────────────────┘
```

#### `a5_cell_to_subcell(cell_id, resolution) -> UBIGINT[]`

Returns the cells at a finer resolution whose **centers lie in** the given cell: the spatial
counterpart of `a5_cell_to_children` and the inverse of `a5_cell_to_supercell`. The subcells of all
the cells at one resolution partition every finer resolution exactly. The result is a
[covering](#coverings-and-compaction-markers).

**Example:**
```sql
SELECT a5_count(a5_cell_to_subcell(a5_lonlat_to_cell(-74.0060, 40.7128, 5), 8)) as subcells, a5_count(a5_cell_to_children(a5_lonlat_to_cell(-74.0060, 40.7128, 5), 8)) as children;
┌──────────┬──────────┐
│ subcells │ children │
│  uint64  │  uint64  │
├──────────┼──────────┤
│       62 │       64 │
└──────────┴──────────┘
```

### Geometric Properties

#### `a5_cell_to_lonlat(cell_id) -> DOUBLE[2]`

Returns the center coordinates [longitude, latitude] of a cell.

**Example:**
```sql
SELECT a5_cell_to_lonlat(3074457264221469552) as center;
┌─────────────────────────────────────────┐
│                 center                  │
│                double[2]                │
├─────────────────────────────────────────┤
│ [122.99924776996579, -52.7390061723111] │
└─────────────────────────────────────────┘
```

#### `a5_cell_to_boundary(cell_id, [closed_ring, [segments]]) -> DOUBLE[2][]`

Returns the boundary vertices of a cell as an array of [longitude, latitude] pairs.

**Parameters:**

- `cell_id` (UBIGINT): The A5 cell
- `closed_ring` (BOOLEAN): Whether to close the ring by repeating the first point at the end. Defaults to true.
- `segments` (INTEGER): Number of segments to use for each edge. If this argument is not supplied or a value is supplied that is <= 0, a resolution-appropriate value will be used.

**Examples:**

```sql
SELECT unnest(a5_cell_to_boundary(3074457264221469552)) as boundary_points;
┌───────────────────────────────────────────┐
│              boundary_points              │
│                 double[2]                 │
├───────────────────────────────────────────┤
│ [122.9992478383981, -52.73900615695749]   │
│ [122.9992477686406, -52.73900615209739]   │
│ [122.99924770219661, -52.739006165912784] │
│ [122.9992477354179, -52.73900619711101]   │
│ [122.99924780517563, -52.73900618947685]  │
│ [122.9992478383981, -52.73900615695749]   │
└───────────────────────────────────────────┘
```

```sql
SELECT unnest(a5_cell_to_boundary(3074457264221469552, false, 5)) as boundary_points;
┌───────────────────────────────────────────┐
│              boundary_points              │
│                 double[2]                 │
├───────────────────────────────────────────┤
│ [122.99924782444657, -52.73900615598549]  │
│ [122.99924781049504, -52.73900615501345]  │
│ [122.99924779654361, -52.73900615404143]  │
│ [122.99924778259208, -52.73900615306941]  │
│ [122.9992477686406, -52.73900615209739]   │
│ [122.9992477553518, -52.739006154860476]  │
│ [122.99924774206306, -52.73900615762355]  │
│ [122.99924772877421, -52.73900616038664]  │
│ [122.99924771548547, -52.739006163149675] │
│ [122.99924770219661, -52.739006165912784] │
│ [122.99924770884093, -52.73900617215241]  │
│ [122.99924771548513, -52.73900617839207]  │
│ [122.99924772212938, -52.7390061846317]   │
│ [122.99924772877364, -52.73900619087136]  │
│ [122.9992477354179, -52.73900619711101]   │
│ [122.99924774936943, -52.739006195584174] │
│ [122.99924776332102, -52.73900619405736]  │
│ [122.99924777727256, -52.739006192530525] │
│ [122.9992477912241, -52.73900619100366]   │
│ [122.99924780517563, -52.73900618947685]  │
│ [122.99924781182011, -52.739006182972965] │
│ [122.9992478184646, -52.7390061764691]    │
│ [122.99924782510908, -52.739006169965265] │
│ [122.99924783175356, -52.73900616346135]  │
│ [122.9992478383981, -52.73900615695749]   │
├───────────────────────────────────────────┤
│                  25 rows                  │
└───────────────────────────────────────────┘
```

### Region Functions

#### `a5_geometry_to_cells(geom, resolution [, overlapping]) -> UBIGINT[]`

Indexes any vector geometry into the set of A5 cells covering it. It builds directly on DuckDB's
built-in `GEOMETRY` type — no `spatial` extension is required (though it composes with it):

- **Points** map to their containing cell.
- **Lines** are traced, in order.
- **Polygons** are filled by **center containment** by default (a cell is included iff its center
  lies inside), with interior rings (**holes**) excluded. Pass `overlapping := true` for **gap-free**
  coverage instead — every cell that touches the polygon boundary is additionally included.
- **`MULTIPOINT` / `MULTILINESTRING` / `MULTIPOLYGON` / `GEOMETRYCOLLECTION`** inputs are unioned.

A geometry with any polygon part is returned as a [covering](#coverings-and-compaction-markers):
compacted cells sorted in curve order, followed by a compaction marker. Point- and line-only
geometries are returned as plain cells at the requested resolution, in geometry order, with no marker.

This is the single entry point for indexing geometries: it is the SQL counterpart of the A5
library's `lonlat_to_cell`, `line_string_to_cells` and `polygon_to_cells`, chosen by geometry type.
(To go the other way — cell → geometry —
see [`a5_cell_to_geometry`](#a5_cell_to_geometrycell_id-segments---geometry).)

**Parameters:**

- `geom` (GEOMETRY): Any geometry
- `resolution` (INTEGER): Resolution level (0-30)
- `overlapping` (BOOLEAN): For polygon parts, include every cell overlapping the boundary (gap-free
  coverage) instead of only cells whose center lies inside. Defaults to `false`.

> **Note:** Polygon coverings are returned **compacted** — cells may be at coarser resolutions where
> a parent is fully contained — and end in a compaction marker, which is not a cell. Use
> [`a5_uncompact`](#a5_uncompactcell_ids---ubigint) to expand to the cells at a
> uniform resolution. Also note that with the default center containment, features smaller than a cell
> (no cell center inside) yield an empty covering (just the marker) — use `overlapping := true` if you
> need those covered instead.

**Examples:**
```sql
-- Index a point, a line, and a polygon
SELECT a5_geometry_to_cells('POINT(-74.0 40.7)'::GEOMETRY, 8) as cell;
SELECT a5_geometry_to_cells('LINESTRING(-74.0 40.7, -73.9 40.8)'::GEOMETRY, 8) as line_cells;
SELECT a5_geometry_to_cells('POLYGON((-74.02 40.70, -73.95 40.70, -73.95 40.78, -74.02 40.78, -74.02 40.70))'::GEOMETRY, 10) as poly_cells;

-- Multi-geometries are unioned
SELECT a5_geometry_to_cells('MULTIPOINT((-74.0 40.7), (-73.9 40.8))'::GEOMETRY, 8) as cells;

-- Expand a polygon covering to a uniform resolution
SELECT a5_uncompact(a5_geometry_to_cells(g, 10)) FROM (SELECT 'POLYGON((-74.02 40.70, -73.95 40.70, -73.95 40.78, -74.02 40.78, -74.02 40.70))'::GEOMETRY AS g);

-- Gap-free coverage: every cell touching the polygon, not just cells whose center is inside
SELECT a5_count(a5_geometry_to_cells('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'::GEOMETRY, 5, true)) as overlapping_cells;
```

**Polygon holes:** Interior rings are excluded from the covering — the a5 crate removes them
directly. At a uniform resolution this simply drops the cells the hole contains:

```sql
SELECT
    len(a5_uncompact(a5_geometry_to_cells('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))'::GEOMETRY, 6))) AS solid_cells,
    len(a5_uncompact(a5_geometry_to_cells('POLYGON((0 0, 10 0, 10 10, 0 10, 0 0), (3 3, 7 3, 7 7, 3 7, 3 3))'::GEOMETRY, 6))) AS with_hole_cells;
┌─────────────┬─────────────────┐
│ solid_cells │ with_hole_cells │
│    int64    │      int64      │
├─────────────┼─────────────────┤
│         150 │             126 │
└─────────────┴─────────────────┘
```

### GEOMETRY Output

These functions turn A5 cells back into DuckDB `GEOMETRY` values, so cells become first-class spatial
objects you can join, measure, and export (e.g. `ST_AsGeoJSON`, `ST_Area`, `ST_Intersects`).

#### `a5_cell_to_geometry(cell_id [, segments]) -> GEOMETRY`

Returns the cell pentagon as a `POLYGON` geometry. The optional `segments` argument interpolates each
edge into that many segments (densification); omit it for the raw 5-vertex pentagon. The world cell
(`a5_world_cell()`) returns `POLYGON EMPTY`.

**Example:**
```sql
SELECT ST_AsText(a5_cell_to_geometry(a5_lonlat_to_cell(-74.0060, 40.7128, 10))) as wkt;
-- POLYGON ((-73.9735... 40.6859..., ...))
```

This makes the GeoJSON example above a one-liner — `a5_cell_to_geometry(cell)` replaces the manual
`ST_MakePolygon(ST_MakeLine(list_transform(a5_cell_to_boundary(cell), ...)))` construction.

#### `a5_cell_to_point(cell_id) -> GEOMETRY`

Returns the cell center as a `POINT` geometry.

**Example:**
```sql
SELECT ST_AsText(a5_cell_to_point(a5_lonlat_to_cell(-74.0060, 40.7128, 10))) as wkt;
-- POINT (-73.9790... 40.7074...)
```

### Traversal Functions

#### `a5_grid_disk(cell_id, k) -> UBIGINT[]`

Returns all A5 cells within `k` edge-steps of the given cell (edge adjacency), including the cell
itself. The result is a [covering](#coverings-and-compaction-markers): compacted cells followed by a
compaction marker — use `a5_uncompact` to get one row per neighbor.

**Parameters:**

- `cell_id` (UBIGINT): The center A5 cell
- `k` (INTEGER): Number of edge-steps (must be >= 0)

**Example:**
```sql
SELECT a5_grid_disk(a5_lonlat_to_cell(-74.0060, 40.7128, 15), 1) as neighbors;
┌───────────────────────────────────────────────────────────────────────────────────────┐
│                                       neighbors                                       │
│                                       uint64[]                                        │
├───────────────────────────────────────────────────────────────────────────────────────┤
│ [2760557102958116864, 2760559170447998976, 2760559181722288128, 17298044693753364544] │
└───────────────────────────────────────────────────────────────────────────────────────┘

SELECT unnest(a5_uncompact(a5_grid_disk(a5_lonlat_to_cell(-74.0060, 40.7128, 15), 1))) as neighbor;
┌─────────────────────┐
│      neighbor       │
│       uint64        │
├─────────────────────┤
│ 2760557102958116864 │
│ 2760559168837386240 │
│ 2760559169911128064 │
│ 2760559170984869888 │
│ 2760559172058611712 │
│ 2760559181722288128 │
└─────────────────────┘
```

#### `a5_grid_disk_vertex(cell_id, k) -> UBIGINT[]`

Returns all A5 cells within `k` vertex-steps of the given cell (vertex adjacency). This returns more cells than `a5_grid_disk` at the same `k` because vertex adjacency includes cells that share only a vertex. The result is a [covering](#coverings-and-compaction-markers).

**Parameters:**

- `cell_id` (UBIGINT): The center A5 cell
- `k` (INTEGER): Number of vertex-steps (must be >= 0)

**Example:**
```sql
SELECT a5_grid_disk_vertex(a5_lonlat_to_cell(-74.0060, 40.7128, 15), 1) as neighbors;
```

#### `a5_spherical_cap(cell_id, radius) -> UBIGINT[]`

Returns all A5 cells whose centers lie within the specified radius (in meters) of the given cell. The
result is a [covering](#coverings-and-compaction-markers).

**Parameters:**

- `cell_id` (UBIGINT): The center A5 cell
- `radius` (DOUBLE): Radius in meters

**Example:**
```sql
SELECT a5_spherical_cap(a5_lonlat_to_cell(-74.0060, 40.7128, 15), 5000.0) as nearby_cells;

-- The compacted list is much shorter than the cells it stands for
SELECT len(cap) as list_length, a5_count(cap) as cells, a5_area(cap) / 1e6 as area_km2 FROM (SELECT a5_spherical_cap(a5_lonlat_to_cell(-74.0060, 40.7128, 15), 5000.0) as cap);
┌─────────────┬────────┬───────────────────┐
│ list_length │ cells  │     area_km2      │
│    int64    │ uint64 │      double       │
├─────────────┼────────┼───────────────────┤
│         249 │   2480 │ 78.53922430755013 │
└─────────────┴────────┴───────────────────┘
```

### Utility Functions

#### `a5_get_num_cells(resolution) -> UBIGINT`

Returns the total number of A5 cells at a given resolution level.

**Example:**
```sql
SELECT a5_get_num_cells(15) as total_cells;
┌─────────────────┐
│   total_cells   │
│     uint64      │
├─────────────────┤
│   16106127360   │
└─────────────────┘
```

#### `a5_get_res0_cells() -> UBIGINT[]`

Returns all 12 base cells at resolution level 0.

**Example:**
```sql
SELECT unnest(a5_get_res0_cells()) as base_cells;
┌─────────────────────┐
│     base_cells      │
│       uint64        │
├─────────────────────┤
│  144115188075855872 │
│  432345564227567616 │
│  720575940379279360 │
│ 1008806316530991104 │
│ 1297036692682702848 │
│ 1585267068834414592 │
│ 1873497444986126336 │
│ 2161727821137838080 │
│ 2449958197289549824 │
│ 2738188573441261568 │
│ 3026418949592973312 │
│ 3314649325744685056 │
├─────────────────────┤
│       12 rows       │
└─────────────────────┘
```

#### `a5_world_cell() -> UBIGINT`

Returns the A5 world cell, the root cell that covers the entire globe and is the ancestor of all resolution-0 cells. Its resolution is reported as `-1` since it sits above resolution 0.

**Example:**
```sql
SELECT a5_world_cell() as world_cell;
┌────────────┐
│ world_cell │
│   uint64   │
├────────────┤
│     0      │
└────────────┘
```

#### `a5_max_resolution() -> INTEGER`

Returns the finest A5 resolution level. Resolutions run from `0` to this value.

**Example:**
```sql
SELECT a5_max_resolution() as max_resolution;
┌────────────────┐
│ max_resolution │
│     int32      │
├────────────────┤
│             30 │
└────────────────┘
```

#### `a5_is_valid_cell(cell_id) -> BOOLEAN`

Returns `true` if the value is a valid A5 cell — i.e. a canonically-encoded cell ID. Useful for
guarding against malformed identifiers (e.g. values from another indexing system, or a `BIGINT`
column that lost its encoding). The world cell (`0`) is considered valid.

The check rejects out-of-range origins and non-canonical bit patterns, including
[compaction markers](#coverings-and-compaction-markers). Note that A5's encoding is dense: most
in-range 64-bit values are a genuine cell, so this is a structural well-formedness check, not a
guarantee that a given number was intentionally produced. In particular it cannot tell a cell ID
created before A5 1.0 from a current one — see [`a5_migrate`](#a5_migratecell_id---ubigint).

**Example:**
```sql
SELECT a5_is_valid_cell(a5_lonlat_to_cell(-74.0060, 40.7128, 15)) as ok,
       a5_is_valid_cell((63::UBIGINT << 58)) as bad_origin;
┌─────────┬────────────┐
│   ok    │ bad_origin │
│ boolean │  boolean   │
├─────────┼────────────┤
│ true    │ false      │
└─────────┴────────────┘
```

#### `a5_hex_to_u64(hex) -> UBIGINT`

Converts an A5 hex string representation to a UBIGINT cell ID.

**Example:**
```sql
SELECT a5_hex_to_u64('1600000000000000') as cell_id;
┌─────────────────────┐
│       cell_id       │
│       uint64        │
├─────────────────────┤
│ 1585267068834414592 │
└─────────────────────┘
```

#### `a5_u64_to_hex(cell_id) -> VARCHAR`

Converts a UBIGINT A5 cell ID to its hex string representation.

**Example:**
```sql
SELECT a5_u64_to_hex(1585267068834414592::ubigint) as hex_id;
┌──────────────────┐
│      hex_id      │
│     varchar      │
├──────────────────┤
│ 1600000000000000 │
└──────────────────┘
```

#### `a5_get_num_children(parent_resolution, child_resolution) -> UBIGINT`

Returns the number of child cells at `child_resolution` that fit within a single cell at `parent_resolution`.

**Example:**
```sql
SELECT a5_get_num_children(0, 1) as num_children;
┌──────────────┐
│ num_children │
│    uint64    │
├──────────────┤
│            5 │
└──────────────┘
```

### Coverings and Compaction Markers

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

#### `a5_compact(cell_ids) -> UBIGINT[]`

Compacts a set of A5 cells by replacing complete groups of sibling cells with their parent cells.
The result is a covering: the compacted cells, then a compaction marker recording the resolution of
the finest input cell.

**Example:**
```sql
SELECT a5_compact([324259173170675712, 396316767208603648, 468374361246531584, 540431955284459520]::ubigint[]) as result;
┌────────────────────────────────────────────┐
│                   result                   │
│                  uint64[]                  │
├────────────────────────────────────────────┤
│ [360287970189639680, 17294385519056126016] │
└────────────────────────────────────────────┘
```

#### `a5_uncompact(cell_ids) -> UBIGINT[]`

Expands a set of A5 cells to all their cells at the resolution of the set — its compaction marker,
or its finest cell when it has none — so `a5_uncompact(a5_compact(cells))` gives back `cells`
(sorted). The result never contains a marker.

**Example:**
```sql
SELECT a5_uncompact([360287970189639680, 17294385519056126016]::ubigint[]) as result;
┌──────────────────────────────────────────────────────────────────────────────────┐
│                                      result                                      │
│                                     uint64[]                                     │
├──────────────────────────────────────────────────────────────────────────────────┤
│ [324259173170675712, 396316767208603648, 468374361246531584, 540431955284459520] │
└──────────────────────────────────────────────────────────────────────────────────┘
```

To expand a single cell to a chosen resolution, use
[`a5_cell_to_children(cell, resolution)`](#a5_cell_to_childrencell_id-target_resolution---ubigint).

#### `a5_is_compaction_marker(value) -> BOOLEAN`

Returns `true` if the value is a compaction marker rather than a cell.

**Example:**
```sql
SELECT a5_is_compaction_marker(c) as is_marker, c FROM (SELECT unnest(a5_compact([324259173170675712, 396316767208603648, 468374361246531584, 540431955284459520]::ubigint[])) as c);
┌───────────┬──────────────────────┐
│ is_marker │          c           │
│  boolean  │        uint64        │
├───────────┼──────────────────────┤
│ false     │   360287970189639680 │
│ true      │ 17294385519056126016 │
└───────────┴──────────────────────┘
```

#### `a5_covering_resolution(cell_ids) -> INTEGER`

Returns the resolution of a set of cells: the resolution of its compaction marker, or of its finest
cell when it has none. Returns `-1` for an empty list.

**Example:**
```sql
SELECT a5_covering_resolution(a5_spherical_cap(a5_lonlat_to_cell(-74.0060, 40.7128, 15), 5000.0)) as resolution;
┌────────────┐
│ resolution │
│   int32    │
├────────────┤
│         15 │
└────────────┘
```

#### `a5_count(cell_ids) -> UBIGINT` and `a5_area(cell_ids) -> DOUBLE`

`a5_count` returns the number of cells in a set at its resolution — the length of the uncompacted
list, without building it. `a5_area` returns the total area of the set in square meters, which is
exact because A5 cells are equal-area. If the list holds overlapping cells (say, a cell and one of
its children) each is counted, so merge such lists with `a5_union` first.

**Example:**
```sql
SELECT len(cap) as list_length, a5_count(cap) as cells, a5_area(cap) / 1e6 as area_km2 FROM (SELECT a5_spherical_cap(a5_lonlat_to_cell(-74.0060, 40.7128, 15), 5000.0) as cap);
┌─────────────┬────────┬───────────────────┐
│ list_length │ cells  │     area_km2      │
│    int64    │ uint64 │      double       │
├─────────────┼────────┼───────────────────┤
│         249 │   2480 │ 78.53922430755013 │
└─────────────┴────────┴───────────────────┘
```

### Set Operations

These operate directly on compacted cells — nothing is uncompacted — so they stay fast for large
regions. Both arguments must be at the **same resolution** (A5 resolutions do not nest
geometrically, so combining sets at different resolutions has no meaning); an error is raised
otherwise.

#### `a5_union(a, b)`, `a5_intersect(a, b)`, `a5_difference(a, b) -> UBIGINT[]`

Return the cells in either set, in both sets, and in `a` but not `b`, as a covering.

**Example:**
```sql
SELECT a5_count(a5_union(a, b)) as in_either, a5_count(a5_intersect(a, b)) as in_both, a5_count(a5_difference(a, b)) as only_in_a FROM (SELECT a5_grid_disk(a5_lonlat_to_cell(-74.0060, 40.7128, 12), 2) as a, a5_spherical_cap(a5_lonlat_to_cell(-73.99, 40.73, 12), 2000.0) as b);
┌───────────┬─────────┬───────────┐
│ in_either │ in_both │ only_in_a │
│  uint64   │ uint64  │  uint64   │
├───────────┼─────────┼───────────┤
│        18 │       6 │        11 │
└───────────┴─────────┴───────────┘
```

#### `a5_overlaps(a, b) -> BOOLEAN`

Returns `true` if the two sets share any cell.

**Example:**
```sql
SELECT a5_overlaps(a5_grid_disk(a5_lonlat_to_cell(-74.0060, 40.7128, 12), 2), a5_spherical_cap(a5_lonlat_to_cell(-73.99, 40.73, 12), 2000.0)) as overlaps;
┌──────────┐
│ overlaps │
│ boolean  │
├──────────┤
│ true     │
└──────────┘
```

#### `a5_contains(cell_ids, cell_id) -> BOOLEAN`

Returns `true` if the cell is in the set. The cell must be at the resolution of the set, so for a
point-in-region test index the point at that resolution. The set must be sorted in curve order, as
coverings are.

**Example:**
```sql
SELECT a5_contains(a5_geometry_to_cells('POLYGON((-74.02 40.70, -73.95 40.70, -73.95 40.78, -74.02 40.78, -74.02 40.70))'::GEOMETRY, 12), a5_lonlat_to_cell(-74.0060, 40.7128, 12)) as inside;
┌─────────┐
│ inside  │
│ boolean │
├─────────┤
│ true    │
└─────────┘
```

### Migration

#### `a5_migrate(cell_id) -> UBIGINT`

Converts a cell ID created before A5 1.0 to the ID of the same cell in the current index. See
[Upgrading to A5 1.0](#-upgrading-to-a5-10).

**Example:**
```sql
-- 2742821848331845632 is what a5_lonlat_to_cell(-74.0060, 40.7128, 15) returned before A5 1.0
SELECT a5_migrate(2742821848331845632) as migrated, a5_lonlat_to_cell(-74.0060, 40.7128, 15) as current;
┌─────────────────────┬─────────────────────┐
│      migrated       │       current       │
│       uint64        │       uint64        │
├─────────────────────┼─────────────────────┤
│ 2760559170984869888 │ 2760559170984869888 │
└─────────────────────┴─────────────────────┘
```

## ⬆️ Upgrading to A5 1.0

This version of the extension is built on A5 1.0, which changes results in ways that matter if you
have stored cell IDs or query code written against an earlier version:

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
  is not a cell — see [Coverings and Compaction Markers](#coverings-and-compaction-markers). Code
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

## 🎯 Resolution Guide

| Resolution | Cell Area (approx) | Use Case |
|------------|-------------------|----------|
| 0-5 | 42M km² - 33k km² | Continental/Country analysis |
| 6-10 | 8k km² - 130 km² | Regional/State analysis |
| 11-15 | 32 km² - 32 hectares | City/District analysis |
| 16-20 | 8 hectares - 124 m² | Neighborhood/Building analysis |
| 21-25 | 31 m² - 0.5 m² | Room/Vehicle analysis |
| 26-30 | 8 cm² - 0.03 mm² | Precision measurements |

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](../LICENSE) file for details.

## 🙏 Credits

- **A5 Algorithm**: This extension utilizes the [`a5`](https://crates.io/crates/a5) Rust crate created by [felixpalmer](https://github.com/felixpalmer)
- **A5 Specification**: Based on the [A5 geospatial index specification](https://a5geo.org)
- **DuckDB Community**: Built on the excellent [DuckDB](https://duckdb.org) database system

## 🔗 Related Links

- [A5 Official Website](https://a5geo.org)
- [A5 Examples and Visualizations](https://a5geo.org/examples/)
