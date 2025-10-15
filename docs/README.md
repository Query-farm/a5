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
SELECT a5_cell(latitude, longitude, 15) as cell_id, COUNT(*) as restaurant_count
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

### Basic Usage

```sql
-- Get the A5 cell for a specific location (latitude, longitude, resolution)
SELECT a5_cell(40.7128, -74.0060, 15) as nyc_cell;  -- Times Square
┌─────────────────────┐
│      nyc_cell       │
│       uint64        │
├─────────────────────┤
│ 2742821848331845632 │
│ (2.74 quintillion)  │
└─────────────────────┘
-- Find the area of that cell in square meters
SELECT a5_area(15) as cell_area_m2;
┌───────────────────┐
│   cell_area_m2    │
│      double       │
├───────────────────┤
│ 31669.04205949599 │
└───────────────────┘

-- Get the center coordinates of a cell
SELECT a5_lat_lon(a5_cell(40.7128, -74.0060, 15)) as center_coords;
┌─────────────────────────────────────────┐
│              center_coords              │
│                double[2]                │
├─────────────────────────────────────────┤
│ [40.71280225138428, -74.00764805615836] │
└─────────────────────────────────────────┘

-- Find parent cell at lower resolution
SELECT a5_parent(a5_cell(40.7128, -74.0060, 15), 10) as parent_cell;
┌─────────────────────┐
│     parent_cell     │
│       uint64        │
├─────────────────────┤
│ 2742821365684895744 │
│ (2.74 quintillion)  │
└─────────────────────┘

-- Get all children cells at higher resolution
SELECT a5_children(a5_cell(40.7128, -74.0060, 10), 11) as child_cells;
┌──────────────────────────────────────────────────────────────────────────────────────┐
│                                     child_cells                                      │
│                                       uint64[]                                       │
├──────────────────────────────────────────────────────────────────────────────────────┤
│ [2742820953368035328, 2742821228245942272, 2742821503123849216, 2742821778001756160] │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

## Code Example: Generate GeoJSON for Cell

To generate a GeoJSON polygon for the A5 cell above use this SQL along with DuckDB's spatial extension:

```sql
SELECT
    ST_AsGeoJSON(
        ST_MakePolygon(
            ST_MakeLine(
                list_transform(
                    a5_boundary(
                        a5_cell(40.41677, -3.7037, 10)
                    ),
                    x -> ST_Point(x[2], x[1])
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
            [-3.639321611065313,40.44502900567739],
            [-3.6973300524360155,40.44427170464865],
            [-3.7459288918337563,40.424159040292615],
            [-3.70791029038422,40.394201800420205],
            [-3.654438659632305,40.4080830654645],
            [-3.639321611065313,40.44502900567739]
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
            [-3.639321611065313,40.44502900567739],
            [-3.6973300524360155,40.44427170464865],
            [-3.7459288918337563,40.424159040292615],
            [-3.70791029038422,40.394201800420205],
            [-3.654438659632305,40.4080830654645],
            [-3.639321611065313,40.44502900567739]
        ]
    ]
}
```


## 📚 API Reference

### Core Functions

#### `a5_cell(latitude, longitude, resolution) -> UBIGINT`

Returns the A5 cell ID for given coordinates and resolution level.

**Parameters:**

- `latitude` (DOUBLE): Latitude in decimal degrees (-90 to 90)
- `longitude` (DOUBLE): Longitude in decimal degrees (-180 to 180)
- `resolution` (INTEGER): Resolution level (0-30, where 0 is coarsest)

**Example:**
```sql
SELECT a5_cell(51.5074, -0.1278, 12) as london_cell;
┌─────────────────────┐
│     london_cell     │
│       uint64        │
├─────────────────────┤
│ 7161033366718906368 │
└─────────────────────┘
```

#### `a5_area(resolution) -> DOUBLE`

Returns the area of an A5 cell in the specified resolution  in square meters.

**Example:**
```sql
SELECT a5_area(5) as area_m2;
┌────────────────────┐
│      area_m2       │
│       double       │
├────────────────────┤
│ 33207397446.578068 │
└────────────────────┘
```

#### `a5_resolution(cell_id) -> INTEGER`

Returns the resolution level of an A5 cell.

**Example:**
```sql
SELECT a5_resolution(207618739568) as resolution;
┌────────────┐
│ resolution │
│   int32    │
├────────────┤
│     27     │
└────────────┘
```

### Spatial Relationships

#### `a5_parent(cell_id, target_resolution) -> UBIGINT`

Returns the parent cell at a coarser resolution level.

**Example:**
```sql
SELECT a5_parent(207618739568, 10) as parent_cell;
┌──────────────────┐
│   parent_cell    │
│      uint64      │
├──────────────────┤
│   549755813888   │
└──────────────────┘
```

#### `a5_children(cell_id, target_resolution) -> UBIGINT[]`

Returns all children cells at a finer resolution level.

**Example:**
```sql
SELECT a5_children(207618739568, 28) as child_cells;
┌──────────────────────────────────────────────────────────┐
│                       child_cells                        │
│                         uint64[]                         │
├──────────────────────────────────────────────────────────┤
│ [207618739528, 207618739544, 207618739560, 207618739576] │
└──────────────────────────────────────────────────────────┘
```

### Geometric Properties

#### `a5_lat_lon(cell_id) -> DOUBLE[2]`

Returns the center coordinates [latitude, longitude] of a cell.

**Example:**
```sql
SELECT a5_lat_lon(207618739568) as center;
┌─────────────────────────────────────────┐
│                 center                  │
│                double[2]                │
├─────────────────────────────────────────┤
│ [52.76769886727584, -129.0078555564143] │
└─────────────────────────────────────────┘
```

#### `a5_boundary(cell_id) -> DOUBLE[2][]`

Returns the boundary vertices of a cell as an array of [latitude, longitude] pairs.

**Example:**
```sql
SELECT unnest(a5_boundary(207618739568)) as boundary_points;
┌───────────────────────────────────────────┐
│              boundary_points              │
│                 double[2]                 │
├───────────────────────────────────────────┤
│ [52.767699205314614, -129.00785542696357] │
│ [52.767698942751544, -129.00785579342767] │
│ [52.76769861890205, -129.0078559316034]   │
│ [52.76769862844177, -129.00785542684645]  │
│ [52.767698940969176, -129.0078552032305]  │
│ [52.767699205314614, -129.00785542696357] │
└───────────────────────────────────────────┘
```

### Utility Functions

#### `a5_num_cells(resolution) -> UBIGINT`

Returns the total number of A5 cells at a given resolution level.

**Example:**
```sql
SELECT a5_num_cells(15) as total_cells;
┌─────────────────┐
│   total_cells   │
│     uint64      │
├─────────────────┤
│   16106127360   │
└─────────────────┘
```

#### `a5_res0_cells() -> UBIGINT[]`

Returns all 12 base cells at resolution level 0.

**Example:**
```sql
SELECT unnest(a5_res0_cells()) as base_cells;
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
