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

### Multi-Variable Spatial Analysis
Correlate different datasets using common cell boundaries:
```sql
-- Compare elevation vs crop yield by region
SELECT
    cells.cell_id,
    AVG(elevation.height) as avg_elevation,
    AVG(crops.yield) as avg_yield
FROM (SELECT DISTINCT a5_cell(lat, lon, 12) as cell_id FROM locations) cells
JOIN elevation ON elevation.cell = cells.cell_id
JOIN crops ON crops.cell = cells.cell_id
GROUP BY cells.cell_id;
```

### Hierarchical Spatial Queries
Navigate between resolution levels for multi-scale analysis:
```sql
-- Find high-resolution hotspots within broader regions
SELECT
    a5_parent(detailed_cell, 10) as region,
    detailed_cell,
    COUNT(*) as point_count
FROM (
    SELECT a5_cell(lat, lon, 15) as detailed_cell
    FROM points
)
GROUP BY region, detailed_cell
HAVING point_count > 100;
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
┌──────────────────────┐
│       nyc_cell       │
│        uint64        │
├──────────────────────┤
│ 13397512237531267072 │
└──────────────────────┘

-- Find the area of that cell in square meters
SELECT a5_area(15) as cell_area_m2;
┌───────────────────┐
│   cell_area_m2    │
│      double       │
├───────────────────┤
│ 31669.04205949599 │
└───────────────────┘

-- Get the center coordinates of a cell
SELECT a5_lon_lat(a5_cell(40.7128, -74.0060, 15)) as center_coords;
┌──────────────────────────────────────────┐
│              center_coords               │
│                double[2]                 │
├──────────────────────────────────────────┤
│ [40.714225512117594, -74.00553999061431] │
└──────────────────────────────────────────┘

-- Find parent cell at lower resolution
SELECT a5_parent(a5_cell(40.7128, -74.0060, 15), 10) as parent_cell;
┌──────────────────────┐
│     parent_cell      │
│        uint64        │
├──────────────────────┤
│ 13397512350811029504 │
└──────────────────────┘

-- Get all children cells at higher resolution
SELECT a5_children(a5_cell(40.7128, -74.0060, 10), 12) as child_cells;
┌──────────────────────────────────────────────────────────────────────────────────────────┐
│                                       child_cells                                        │
│                                         uint64[]                                         │
├──────────────────────────────────────────────────────────────────────────────────────────┤
│ [13397511938494169088, 13397512213372076032, 13397512488249982976, 13397512763127889920] │
└──────────────────────────────────────────────────────────────────────────────────────────┘
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
SELECT a5_cell(51.5074, -0.1278, 12) as london_cell;  -- 207618739568
```

#### `a5_area(resolution) -> DOUBLE`

Returns the area of an A5 cell in the specified resolution  in square meters.

**Example:**
```sql
SELECT a5_area(5) as area_m2;
```

#### `a5_resolution(cell_id) -> INTEGER`

Returns the resolution level of an A5 cell.

**Example:**
```sql
SELECT a5_resolution(207618739568) as resolution;  -- 12
```

### Spatial Relationships

#### `a5_parent(cell_id, target_resolution) -> UBIGINT`

Returns the parent cell at a coarser resolution level.

**Example:**
```sql
SELECT a5_parent(207618739568, 10) as parent_cell;
```

#### `a5_children(cell_id, target_resolution) -> UBIGINT[]`

Returns all children cells at a finer resolution level.

**Example:**
```sql
SELECT a5_children(207618739568, 14) as child_cells;
```

### Geometric Properties

#### `a5_lon_lat(cell_id) -> DOUBLE[2]`

Returns the center coordinates [longitude, latitude] of a cell.

**Example:**
```sql
SELECT a5_lon_lat(207618739568) as center;  -- [-0.1278, 51.5074]
```

#### `a5_boundary(cell_id) -> DOUBLE[2][]`

Returns the boundary vertices of a cell as an array of [longitude, latitude] pairs.

**Example:**
```sql
SELECT a5_boundary(207618739568) as boundary_points;
```

### Utility Functions

#### `a5_num_cells(resolution) -> UBIGINT`

Returns the total number of A5 cells at a given resolution level.

**Example:**
```sql
SELECT a5_num_cells(15) as total_cells;  -- 16,106,127,360
```

#### `a5_res0_cells() -> UBIGINT[]`

Returns all 12 base cells at resolution level 0.

**Example:**
```sql
SELECT a5_res0_cells() as base_cells;
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
