use a5;
use std::ffi::{CStr, CString};

#[repr(C)]
pub struct ResultU64 {
    pub value: u64,
    pub error: *mut std::os::raw::c_char, // null if no error
}

#[repr(C)]
pub struct ResultLonLat {
    pub longitude: f64,
    pub latitude: f64,
    pub error: *mut std::os::raw::c_char, // null if no error
}

#[repr(C)]
pub struct ResultBool {
    pub value: bool,
    pub error: *mut std::os::raw::c_char, // null if no error
}

#[repr(C)]
pub struct ResultF64 {
    pub value: f64,
    pub error: *mut std::os::raw::c_char, // null if no error
}

#[repr(C)]
pub struct CellBoundaryOptions {
    pub closed_ring: bool,
    /// Number of segments to use for each edge. Pass None to use the resolution of the cell (default: None)
    pub segments: i32,
}


impl CellBoundaryOptions {
    pub fn new(closed_ring: bool, segments: Option<i32>) -> Self {
        Self { closed_ring, segments: segments.unwrap_or(-1) }
    }

    pub fn segments(&self) -> Option<i32> {
        if self.segments < 0 { None } else { Some(self.segments) }
    }
}


// A panic must not unwind across the FFI boundary - it aborts the host process - so a panic
// inside the a5 crate (e.g. on an input it does not expect, like the world cell in a
// traversal) is reported as an ordinary error instead.
fn guard<T>(f: impl FnOnce() -> Result<T, String>) -> Result<T, String> {
    match std::panic::catch_unwind(std::panic::AssertUnwindSafe(f)) {
        Ok(result) => result,
        Err(panic) => {
            let reason = panic
                .downcast_ref::<&str>()
                .map(|s| s.to_string())
                .or_else(|| panic.downcast_ref::<String>().cloned())
                .unwrap_or_else(|| "unknown panic".to_string());
            Err(format!("a5 internal error: {}", reason))
        }
    }
}

#[no_mangle]
pub extern "C" fn a5_lon_lat_to_cell(longitude: f64, latitude: f64, resolution: i32) -> ResultU64 {
    match guard(|| a5::lonlat_to_cell(a5::LonLat::new(longitude, latitude), resolution)) {
        Ok(cell) => ResultU64 { value: cell, error: std::ptr::null_mut() },
        Err(e) => {
            let err_msg = std::ffi::CString::new(e.to_string()).unwrap();
            ResultU64 { value: 0, error: err_msg.into_raw() }
        }
    }
}

#[no_mangle]
pub extern "C" fn a5_cell_to_parent(index: u64, parent_resolution: i32) -> ResultU64 {
    match guard(|| a5::cell_to_parent(index, Some(parent_resolution))) {
        Ok(cell) => ResultU64 { value: cell, error: std::ptr::null_mut() },
        Err(e) => {
            let err_msg = std::ffi::CString::new(e.to_string()).unwrap();
            ResultU64 { value: 0, error: err_msg.into_raw() }
        }
    }
}

// The parent one resolution coarser (a5::cell_to_parent with no resolution given).
#[no_mangle]
pub extern "C" fn a5_cell_to_immediate_parent(index: u64) -> ResultU64 {
    u64_result_to_c(guard(|| a5::cell_to_parent(index, None)))
}


#[no_mangle]
pub extern "C" fn a5_cell_area(resolution: i32) -> f64 {
    a5::cell_area(resolution)
}

#[no_mangle]
pub extern "C" fn a5_cell_edge_length_avg(resolution: i32) -> f64 {
    a5::cell_edge_length_avg(resolution)
}

#[no_mangle]
pub extern "C" fn a5_cell_to_lon_lat(cell: u64) -> ResultLonLat {
    match guard(|| a5::cell_to_lonlat(cell)) {
        Ok(lonlat) => ResultLonLat { longitude: lonlat.longitude.get(), latitude: lonlat.latitude.get(), error: std::ptr::null_mut() },
        Err(e) => {
            let err_msg = std::ffi::CString::new(e.to_string()).unwrap();
            ResultLonLat { longitude: 0.0, latitude: 0.0, error: err_msg.into_raw() }
        }
    }
}

#[no_mangle]
pub extern "C" fn a5_get_num_cells(resolution: i32) -> u64 {
    a5::get_num_cells(resolution)
}

#[no_mangle]
pub extern "C" fn a5_get_resolution(index: u64) -> i32 {
    a5::get_resolution(index)
}

#[repr(C)]
#[derive(Debug, Copy, Clone)]
pub struct LonLatDegrees {
    pub lon: f64,
    pub lat: f64,
}

#[repr(C)]
pub struct LonLatDegreesArray {
    pub data: *mut LonLatDegrees,        // pointer to array of LonLatDegrees
    pub len: usize,               // length of the array
    pub error: *mut std::os::raw::c_char, // null if no error
}

#[repr(C)]
pub struct CellArray {
    pub data: *mut u64,        // pointer to array of cell IDs
    pub len: usize,               // length of the array
    pub error: *mut std::os::raw::c_char, // null if no error
}


pub fn vec_result_to_c(result: Result<Vec<a5::LonLat>, String>) -> LonLatDegreesArray {
    match result {
        Ok(vec) => {
            let degree_vec: Vec<LonLatDegrees> = vec.into_iter().map(|ll| LonLatDegrees { lon: ll.longitude.get(), lat: ll.latitude.get() }).collect();
            let len = degree_vec.len();

            let mut boxed_slice = degree_vec.into_boxed_slice(); // heap allocation
            let data_ptr = boxed_slice.as_mut_ptr();
            std::mem::forget(boxed_slice); // prevent Rust from freeing it
            LonLatDegreesArray { data: data_ptr, len, error: std::ptr::null_mut() }
        }
        Err(e) => {
            let c_str = CString::new(e).unwrap();
            LonLatDegreesArray { data: std::ptr::null_mut(), len: 0, error: c_str.into_raw() }
        }
    }
}

pub fn cell_vec_result_to_c(result: Result<Vec<u64>, String>) -> CellArray {
    match result {
        Ok(vec) => {
            let mut boxed_slice = vec.into_boxed_slice(); // heap allocation
            let data_ptr = boxed_slice.as_mut_ptr();
            let len = boxed_slice.len();
            std::mem::forget(boxed_slice); // prevent Rust from freeing it
            CellArray { data: data_ptr, len, error: std::ptr::null_mut() }
        }
        Err(e) => {
            let c_str = CString::new(e).unwrap();
            CellArray { data: std::ptr::null_mut(), len: 0, error: c_str.into_raw() }
        }
    }
}


#[no_mangle]
pub extern "C" fn a5_free_lonlatdegrees_array(arr: LonLatDegreesArray) {
    if !arr.data.is_null() {
        unsafe {
            // reconstruct the boxed slice and drop it
            let _ = Box::from_raw(std::slice::from_raw_parts_mut(arr.data, arr.len));
        }
    }
    if !arr.error.is_null() {
        unsafe { drop(CString::from_raw(arr.error)); }
    }
}

#[no_mangle]
pub extern "C" fn a5_free_cell_array(arr: CellArray) {
    if !arr.data.is_null() {
        unsafe {
            // reconstruct the boxed slice and drop it
            let _ = Box::from_raw(std::slice::from_raw_parts_mut(arr.data, arr.len));
        }
    }
    if !arr.error.is_null() {
        unsafe { drop(CString::from_raw(arr.error)); }
    }
}

#[no_mangle]
pub extern "C" fn a5_cell_to_boundary(cell_id: u64, options: CellBoundaryOptions) -> LonLatDegreesArray {
    vec_result_to_c(guard(|| a5::cell_to_boundary(cell_id, Some(a5::CellToBoundaryOptions { closed_ring: options.closed_ring, segments: options.segments() }))))
}

#[no_mangle]
pub extern "C" fn a5_cell_to_children(index: u64, child_resolution: i32) -> CellArray {
    match child_resolution {
        r if r >= 0 && r < 31 => {
            cell_vec_result_to_c(guard(|| a5::cell_to_children(index, Some(child_resolution))))
        }
        _ => cell_vec_result_to_c(guard(|| a5::cell_to_children(index, None))),
    }
}

#[no_mangle]
pub extern "C" fn a5_get_res0_cells() -> CellArray {
    cell_vec_result_to_c(guard(|| a5::get_res0_cells()))
}

#[no_mangle]
pub extern "C" fn a5_compact(cells: *const u64, len: usize) -> CellArray {
    if cells.is_null() || len == 0 {
        return CellArray { data: std::ptr::null_mut(), len: 0, error: std::ptr::null_mut() };
    }
    let cell_slice = unsafe { std::slice::from_raw_parts(cells, len) };
    cell_vec_result_to_c(guard(|| a5::compact(cell_slice)))
}

#[no_mangle]
pub extern "C" fn a5_uncompact(cells: *const u64, len: usize) -> CellArray {
    if cells.is_null() || len == 0 {
        return CellArray { data: std::ptr::null_mut(), len: 0, error: std::ptr::null_mut() };
    }
    let cell_slice = unsafe { std::slice::from_raw_parts(cells, len) };
    cell_vec_result_to_c(guard(|| a5::uncompact(cell_slice)))
}

#[no_mangle]
pub extern "C" fn a5_free_string(ptr: *mut std::os::raw::c_char) {
    if !ptr.is_null() {
        unsafe { drop(CString::from_raw(ptr)); }
    }
}

#[no_mangle]
pub extern "C" fn a5_hex_to_u64(hex: *const std::os::raw::c_char) -> ResultU64 {
    if hex.is_null() {
        let err_msg = CString::new("hex string is null").unwrap();
        return ResultU64 { value: 0, error: err_msg.into_raw() };
    }
    let c_str = unsafe { CStr::from_ptr(hex) };
    let hex_str = match c_str.to_str() {
        Ok(s) => s,
        Err(e) => {
            let err_msg = CString::new(e.to_string()).unwrap();
            return ResultU64 { value: 0, error: err_msg.into_raw() };
        }
    };
    match guard(|| a5::hex_to_u64(hex_str)) {
        Ok(value) => ResultU64 { value, error: std::ptr::null_mut() },
        Err(e) => {
            let err_msg = CString::new(e.to_string()).unwrap();
            ResultU64 { value: 0, error: err_msg.into_raw() }
        }
    }
}

#[no_mangle]
pub extern "C" fn a5_u64_to_hex(value: u64) -> *mut std::os::raw::c_char {
    let hex = a5::u64_to_hex(value);
    CString::new(hex).unwrap().into_raw()
}

#[no_mangle]
pub extern "C" fn a5_get_num_children(parent_res: i32, child_res: i32) -> usize {
    a5::get_num_children(parent_res, child_res)
}

#[no_mangle]
pub extern "C" fn a5_spherical_cap(cell_id: u64, radius: f64) -> CellArray {
    cell_vec_result_to_c(guard(|| a5::spherical_cap(cell_id, radius)))
}

#[no_mangle]
pub extern "C" fn a5_grid_disk(cell_id: u64, k: usize) -> CellArray {
    cell_vec_result_to_c(guard(|| a5::grid_disk(cell_id, k)))
}

#[no_mangle]
pub extern "C" fn a5_grid_disk_vertex(cell_id: u64, k: usize) -> CellArray {
    cell_vec_result_to_c(guard(|| a5::grid_disk_vertex(cell_id, k)))
}

#[no_mangle]
pub extern "C" fn a5_max_resolution() -> i32 {
    a5::MAX_RESOLUTION
}

#[no_mangle]
pub extern "C" fn a5_world_cell() -> u64 {
    a5::WORLD_CELL
}

#[no_mangle]
pub extern "C" fn a5_is_valid_cell(index: u64) -> bool {
    a5::is_valid_cell(index)
}

// Build a Vec<a5::LonLat> from a C array of LonLatDegrees (lon, lat) input points.
fn lonlat_slice_to_vec(points: *const LonLatDegrees, len: usize) -> Vec<a5::LonLat> {
    let slice = unsafe { std::slice::from_raw_parts(points, len) };
    slice.iter().map(|p| a5::LonLat::new(p.lon, p.lat)).collect()
}

#[no_mangle]
pub extern "C" fn a5_line_string_to_cells(points: *const LonLatDegrees, len: usize, resolution: i32) -> CellArray {
    if points.is_null() || len == 0 {
        return CellArray { data: std::ptr::null_mut(), len: 0, error: std::ptr::null_mut() };
    }
    let lonlats = lonlat_slice_to_vec(points, len);
    cell_vec_result_to_c(guard(|| a5::line_string_to_cells(&lonlats, resolution)))
}

// GeoJSON-style polygon: ring 0 is the outer ring, rings 1.. are holes. The
// rings are passed flattened into a single `points` buffer, with `ring_lengths`
// giving the vertex count of each ring (so the offsets can be reconstructed).
// Holes are excluded by the a5 crate itself - the caller does no hole handling.
// The result is a covering: compacted cells followed by a compaction marker.
// `overlapping` selects a5's Containment::Overlapping (every cell touching the
// polygon, for gap-free coverage) instead of the default Containment::Center
// (cell center inside the polygon).
#[no_mangle]
pub extern "C" fn a5_polygon_to_cells(
    points: *const LonLatDegrees,
    ring_lengths: *const usize,
    ring_count: usize,
    resolution: i32,
    overlapping: bool,
) -> CellArray {
    if points.is_null() || ring_lengths.is_null() || ring_count == 0 {
        return CellArray { data: std::ptr::null_mut(), len: 0, error: std::ptr::null_mut() };
    }
    let lengths = unsafe { std::slice::from_raw_parts(ring_lengths, ring_count) };
    let total: usize = lengths.iter().sum();
    let flat = unsafe { std::slice::from_raw_parts(points, total) };

    let mut rings: Vec<Vec<a5::LonLat>> = Vec::with_capacity(ring_count);
    let mut offset = 0;
    for &len in lengths {
        rings.push(
            flat[offset..offset + len]
                .iter()
                .map(|p| a5::LonLat::new(p.lon, p.lat))
                .collect(),
        );
        offset += len;
    }
    let containment = if overlapping {
        a5::Containment::Overlapping
    } else {
        a5::Containment::Center
    };
    let options = a5::PolygonToCellsOptions { containment };
    cell_vec_result_to_c(guard(|| a5::polygon_to_cells(&rings, resolution, Some(options))))
}


fn u64_result_to_c(result: Result<u64, String>) -> ResultU64 {
    match result {
        Ok(value) => ResultU64 { value, error: std::ptr::null_mut() },
        Err(e) => ResultU64 { value: 0, error: CString::new(e).unwrap().into_raw() },
    }
}

fn bool_result_to_c(result: Result<bool, String>) -> ResultBool {
    match result {
        Ok(value) => ResultBool { value, error: std::ptr::null_mut() },
        Err(e) => ResultBool { value: false, error: CString::new(e).unwrap().into_raw() },
    }
}

// Borrow a C array of cells as a slice; a null pointer or zero length is the empty set.
fn cells_to_slice<'a>(cells: *const u64, len: usize) -> &'a [u64] {
    if cells.is_null() || len == 0 {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(cells, len) }
    }
}

#[no_mangle]
pub extern "C" fn a5_is_compaction_marker(value: u64) -> bool {
    a5::is_compaction_marker(value)
}

#[no_mangle]
pub extern "C" fn a5_covering_resolution(cells: *const u64, len: usize) -> i32 {
    a5::covering_resolution(cells_to_slice(cells, len))
}

#[no_mangle]
pub extern "C" fn a5_count(cells: *const u64, len: usize) -> ResultU64 {
    u64_result_to_c(guard(|| a5::count(cells_to_slice(cells, len))))
}

#[no_mangle]
pub extern "C" fn a5_area(cells: *const u64, len: usize) -> ResultF64 {
    match guard(|| a5::area(cells_to_slice(cells, len))) {
        Ok(value) => ResultF64 { value, error: std::ptr::null_mut() },
        Err(e) => ResultF64 { value: 0.0, error: CString::new(e).unwrap().into_raw() },
    }
}

#[no_mangle]
pub extern "C" fn a5_union(a: *const u64, a_len: usize, b: *const u64, b_len: usize) -> CellArray {
    cell_vec_result_to_c(guard(|| a5::union(cells_to_slice(a, a_len), cells_to_slice(b, b_len))))
}

#[no_mangle]
pub extern "C" fn a5_intersect(a: *const u64, a_len: usize, b: *const u64, b_len: usize) -> CellArray {
    cell_vec_result_to_c(guard(|| a5::intersect(cells_to_slice(a, a_len), cells_to_slice(b, b_len))))
}

#[no_mangle]
pub extern "C" fn a5_difference(a: *const u64, a_len: usize, b: *const u64, b_len: usize) -> CellArray {
    cell_vec_result_to_c(guard(|| a5::difference(cells_to_slice(a, a_len), cells_to_slice(b, b_len))))
}

#[no_mangle]
pub extern "C" fn a5_overlaps(a: *const u64, a_len: usize, b: *const u64, b_len: usize) -> ResultBool {
    bool_result_to_c(guard(|| a5::overlaps(cells_to_slice(a, a_len), cells_to_slice(b, b_len))))
}

#[no_mangle]
pub extern "C" fn a5_contains(cells: *const u64, len: usize, cell: u64) -> ResultBool {
    bool_result_to_c(guard(|| a5::contains(cells_to_slice(cells, len), cell)))
}

#[no_mangle]
pub extern "C" fn a5_cell_to_supercell(cell: u64, resolution: i32) -> ResultU64 {
    u64_result_to_c(guard(|| a5::cell_to_supercell(cell, resolution)))
}

#[no_mangle]
pub extern "C" fn a5_cell_to_subcell(cell: u64, resolution: i32) -> CellArray {
    cell_vec_result_to_c(guard(|| a5::cell_to_subcell(cell, resolution)))
}

// Migrates a cell ID from the v0 index (a5 <= 0.10) to the v1 index (a5 >= 1.0).
#[no_mangle]
pub extern "C" fn a5_migrate(cell: u64) -> ResultU64 {
    u64_result_to_c(guard(|| a5::migrate(cell)))
}
