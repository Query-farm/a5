use a5;
use std::ffi::CString;


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

#[unsafe(no_mangle)]
pub extern "C" fn lon_lat_to_cell(longitude: f64, latitude: f64, resolution: i32) -> ResultU64 {
    match a5::lonlat_to_cell(a5::LonLat::new(longitude, latitude), resolution) {
        Ok(cell) => ResultU64 { value: cell, error: std::ptr::null_mut() },
        Err(e) => {
            let err_msg = std::ffi::CString::new(e.to_string()).unwrap();
            ResultU64 { value: 0, error: err_msg.into_raw() }
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn cell_to_parent(index: u64, parent_resolution: i32) -> ResultU64 {
    match a5::cell_to_parent(index, Some(parent_resolution)) {
        Ok(cell) => ResultU64 { value: cell, error: std::ptr::null_mut() },
        Err(e) => {
            let err_msg = std::ffi::CString::new(e.to_string()).unwrap();
            ResultU64 { value: 0, error: err_msg.into_raw() }
        }
    }
}


#[unsafe(no_mangle)]
pub extern "C" fn cell_area(resolution: i32) -> f64 {
    a5::cell_area(resolution)
}

#[unsafe(no_mangle)]
pub extern "C" fn cell_to_lon_lat(cell: u64) -> ResultLonLat {
    match a5::cell_to_lonlat(cell) {
        Ok(lonlat) => ResultLonLat { longitude: lonlat.longitude.get(), latitude: lonlat.latitude.get(), error: std::ptr::null_mut() },
        Err(e) => {
            let err_msg = std::ffi::CString::new(e.to_string()).unwrap();
            ResultLonLat { longitude: 0.0, latitude: 0.0, error: err_msg.into_raw() }
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn get_num_cells(resolution: i32) -> u64 {
    a5::get_num_cells(resolution)
}

#[unsafe(no_mangle)]
pub extern "C" fn get_resolution(index: u64) -> i32 {
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
            let mut boxed_slice = degree_vec.into_boxed_slice(); // heap allocation
            let data_ptr = boxed_slice.as_mut_ptr();
            let len = boxed_slice.len();
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


#[unsafe(no_mangle)]
pub extern "C" fn free_lonlatdegrees_array(arr: LonLatDegreesArray) {
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

#[unsafe(no_mangle)]
pub extern "C" fn free_cell_array(arr: CellArray) {
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



#[unsafe(no_mangle)]
pub extern "C" fn cell_to_boundary(cell_id: u64) -> LonLatDegreesArray {
    vec_result_to_c(a5::cell_to_boundary(cell_id, None))
}

#[unsafe(no_mangle)]
pub extern "C" fn cell_to_children(index: u64, child_resolution: i32) -> CellArray {
    match child_resolution {
        r if r < 0 || r > 29 => {
            cell_vec_result_to_c(a5::cell_to_children(index, Some(child_resolution)))
        }
        _ => cell_vec_result_to_c(a5::cell_to_children(index, None)),
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn get_res0_cells() -> CellArray {
    cell_vec_result_to_c(a5::get_res0_cells())
}

