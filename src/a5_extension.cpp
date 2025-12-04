#define DUCKDB_EXTENSION_MAIN

#include "a5_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/function/scalar_function.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>
#include "rust.h"
#include "query_farm_telemetry.hpp"
namespace duckdb {

#define MAX_RESOLUTION 30

// Helper function to validate resolution and throw with a clear error message
inline void ValidateResolution(int32_t resolution, const char *function_name) {
	if (resolution < 0 || resolution > MAX_RESOLUTION) {
		throw InvalidInputException(string(function_name) + ": Resolution must be between 0 and 30");
	}
}

// Helper function to safely throw with error from Rust, freeing the error string
inline void ThrowRustError(char *error_ptr, const char *function_name) {
	if (error_ptr != nullptr) {
		string error_msg = string(function_name) + ": " + string(error_ptr);
		free(error_ptr);
		throw InvalidInputException(error_msg);
	}
}

// Helper function to check CellArray for error, free it, and throw
inline void ThrowCellArrayError(CellArray &arr, const char *function_name) {
	if (arr.error) {
		string error_msg = string(function_name) + ": " + string(arr.error);
		a5_free_cell_array(arr);
		throw InvalidInputException(error_msg);
	}
}

// Helper function to check LonLatDegreesArray for error, free it, and throw
inline void ThrowLonLatArrayError(LonLatDegreesArray &arr, const char *function_name) {
	if (arr.error) {
		string error_msg = string(function_name) + ": " + string(arr.error);
		a5_free_lonlatdegrees_array(arr);
		throw InvalidInputException(error_msg);
	}
}

inline void A5CellAreaFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &resolution_vector = args.data[0];
	UnaryExecutor::Execute<int32_t, double>(resolution_vector, result, args.size(), [&](int32_t resolution) {
		ValidateResolution(resolution, "a5_cell_area");
		return a5_cell_area(resolution);
	});
}

inline void A5GetNumCellsFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &resolution_vector = args.data[0];
	UnaryExecutor::Execute<int32_t, uint64_t>(resolution_vector, result, args.size(), [&](int32_t resolution) {
		ValidateResolution(resolution, "a5_get_num_cells");
		return a5_get_num_cells(resolution);
	});
}

inline void A5GetResolutionFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &cell_vector = args.data[0];
	UnaryExecutor::Execute<uint64_t, int32_t>(cell_vector, result, args.size(),
	                                          [&](uint64_t cell) { return a5_get_resolution(cell); });
}

inline void A5LonLatToCellFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &lon_vector = args.data[0];
	auto &lat_vector = args.data[1];
	auto &resolution_vector = args.data[2];

	TernaryExecutor::Execute<double, double, int32_t, uint64_t>(
	    lon_vector, lat_vector, resolution_vector, result, args.size(),
	    [&](double lon, double lat, int32_t resolution) {
		    ValidateResolution(resolution, "a5_lonlat_to_cell");
		    struct ResultU64 res = a5_lon_lat_to_cell(lon, lat, resolution);
		    ThrowRustError(res.error, "a5_lonlat_to_cell");
		    return res.value;
	    });
}

inline void A5CellToParentFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &cell_vector = args.data[0];
	auto &parent_resolution_vector = args.data[1];

	BinaryExecutor::Execute<uint64_t, int32_t, uint64_t>(
	    cell_vector, parent_resolution_vector, result, args.size(), [&](uint64_t cell, int32_t parent_resolution) {
		    ValidateResolution(parent_resolution, "a5_cell_to_parent");
		    struct ResultU64 res = a5_cell_to_parent(cell, parent_resolution);
		    ThrowRustError(res.error, "a5_cell_to_parent");
		    return res.value;
	    });
}

inline void A5CellToLonLatFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &cell_vector = args.data[0];

	auto &result_data_children = ArrayVector::GetEntry(result);
	double *data_ptr = FlatVector::GetData<double>(result_data_children);

	// Standardize the vectors to a unified format, so it can be iterated.
	UnifiedVectorFormat cell_id_format;
	cell_vector.ToUnifiedFormat(args.size(), cell_id_format);

	uint64_t *input_data_ptr = FlatVector::GetData<uint64_t>(cell_vector);

	for (idx_t i = 0; i < args.size(); i++) {
		auto cell_idx = cell_id_format.sel->get_index(i);

		// If the input value is NULL then the output value should be NULL.
		if (!cell_id_format.validity.RowIsValid(cell_idx)) {
			FlatVector::SetNull(result, i, true);
			continue;
		}

		struct ResultLonLat res = a5_cell_to_lon_lat(input_data_ptr[cell_idx]);
		ThrowRustError(res.error, "a5_cell_to_lonlat");

		data_ptr[i * 2] = res.longitude;
		data_ptr[i * 2 + 1] = res.latitude;
	}

	if (args.size() == 1) {
		result.SetVectorType(VectorType::CONSTANT_VECTOR);
	}
}

inline void A5CellToChildrenFun(DataChunk &args, ExpressionState &state, Vector &result) {
	// A5 cells have exactly 4 children
	ListVector::Reserve(result, args.size() * 4);
	uint64_t offset = 0;

	if (args.ColumnCount() == 2) {
		auto &cell_vector = args.data[0];
		auto &max_resolution_vector = args.data[1];

		BinaryExecutor::Execute<uint64_t, int32_t, list_entry_t>(
		    cell_vector, max_resolution_vector, result, args.size(), [&](uint64_t cell_id, int32_t child_resolution) {
			    ValidateResolution(child_resolution, "a5_cell_to_children");
			    auto child_result = a5_cell_to_children(cell_id, child_resolution);
			    ThrowCellArrayError(child_result, "a5_cell_to_children");

			    if (child_result.len == 0) {
				    a5_free_cell_array(child_result);
				    return list_entry_t {0, 0};
			    }
			    for (size_t i = 0; i < child_result.len; i++) {
				    ListVector::PushBack(result, Value::UBIGINT(child_result.data[i]));
			    }
			    list_entry_t out {offset, child_result.len};
			    offset += child_result.len;
			    a5_free_cell_array(child_result);
			    return out;
		    });
	} else if (args.ColumnCount() == 1) {
		auto &cell_vector = args.data[0];

		UnaryExecutor::Execute<uint64_t, list_entry_t>(cell_vector, result, args.size(), [&](uint64_t cell_id) {
			auto child_result = a5_cell_to_children(cell_id, -1);
			ThrowCellArrayError(child_result, "a5_cell_to_children");

			if (child_result.len == 0) {
				a5_free_cell_array(child_result);
				return list_entry_t {0, 0};
			}
			for (size_t i = 0; i < child_result.len; i++) {
				ListVector::PushBack(result, Value::UBIGINT(child_result.data[i]));
			}
			list_entry_t out {offset, child_result.len};
			offset += child_result.len;
			a5_free_cell_array(child_result);
			return out;
		});
	} else {
		throw InvalidInputException("A5CellToChildrenFun: expected 1 or 2 arguments.");
	}
}

inline void A5CellToBoundaryFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &cell_vector = args.data[0];
	// A5 cells are pentagons with 5 vertices
	ListVector::Reserve(result, args.size() * 5);
	uint64_t offset = 0;

	auto compute_boundary = [&](uint64_t cell_id, bool closed_ring, int32_t segments) -> list_entry_t {
		if (cell_id == 0) {
			// A5 defines cell 0 as invalid / non-existent, so return an empty boundary
			return {0, 0};
		}

		CellBoundaryOptions options;
		options.closed_ring = closed_ring;
		options.segments = segments;

		auto boundary_result = a5_cell_to_boundary(cell_id, options);
		ThrowLonLatArrayError(boundary_result, "a5_cell_to_boundary");
		if (boundary_result.len == 0) {
			a5_free_lonlatdegrees_array(boundary_result);
			return {0, 0};
		}

		for (size_t i = 0; i < boundary_result.len; i++) {
			auto &coord = boundary_result.data[i];
			ListVector::PushBack(
			    result, Value::ARRAY(LogicalType::DOUBLE, {Value::DOUBLE(coord.lon), Value::DOUBLE(coord.lat)}));
		}

		list_entry_t out {offset, boundary_result.len};
		offset += boundary_result.len;
		a5_free_lonlatdegrees_array(boundary_result);
		return out;
	};

	if (args.ColumnCount() == 1) {
		UnaryExecutor::Execute<uint64_t, list_entry_t>(
		    cell_vector, result, args.size(), [&](uint64_t cell_id) { return compute_boundary(cell_id, true, -1); });
	} else if (args.ColumnCount() == 2) {
		auto &closed_ring_vector = args.data[1];
		BinaryExecutor::Execute<uint64_t, bool, list_entry_t>(
		    cell_vector, closed_ring_vector, result, args.size(),
		    [&](uint64_t cell_id, bool closed_ring) { return compute_boundary(cell_id, closed_ring, -1); });
	} else if (args.ColumnCount() == 3) {
		auto &closed_ring_vector = args.data[1];
		auto &segments_vector = args.data[2];
		TernaryExecutor::Execute<uint64_t, bool, int32_t, list_entry_t>(
		    cell_vector, closed_ring_vector, segments_vector, result, args.size(),
		    [&](uint64_t cell_id, bool closed_ring, int32_t segments) {
			    if (segments <= 0) {
				    segments = -1;
			    }
			    return compute_boundary(cell_id, closed_ring, segments);
		    });
	} else {
		throw InvalidInputException("A5CellToBoundaryFun: expected 1, 2 or 3 arguments.");
	}
}

inline void A5GetRes0CellsFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto cells = a5_get_res0_cells();
	vector<Value> cell_vec;
	for (size_t i = 0; i < cells.len; i++) {
		cell_vec.emplace_back(Value::UBIGINT(cells.data[i]));
	}
	a5_free_cell_array(cells);
	Value val = Value::LIST(LogicalType::UBIGINT, cell_vec);

	D_ASSERT(args.ColumnCount() == 0);
	result.Reference(val);
}

inline void A5CompactFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &cell_list_vector = args.data[0];

	// Initial estimate; compacted output is typically smaller than input
	ListVector::Reserve(result, args.size() * 4);
	uint64_t offset = 0;

	auto cell_list_data = FlatVector::GetData<uint64_t>(ListVector::GetEntry(cell_list_vector));

	auto result_size = ListVector::GetListSize(result);

	UnaryExecutor::Execute<list_entry_t, list_entry_t>(
	    cell_list_vector, result, args.size(), [&](list_entry_t cell_list_entry) {
		    // We need to prepare the list of values to pass in.
		    auto compact_result = a5_compact(cell_list_data + cell_list_entry.offset, cell_list_entry.length);
		    ThrowCellArrayError(compact_result, "a5_compact");
		    if (compact_result.len == 0) {
			    a5_free_cell_array(compact_result);
			    return list_entry_t {0, 0};
		    }
		    ListVector::Reserve(result, result_size + compact_result.len);
		    for (size_t i = 0; i < compact_result.len; i++) {
			    ListVector::PushBack(result, Value::UBIGINT(compact_result.data[i]));
		    }
		    result_size += compact_result.len;

		    list_entry_t out {offset, compact_result.len};
		    offset += compact_result.len;
		    a5_free_cell_array(compact_result);
		    return out;
	    });
}

inline void A5UncompactFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &cell_list_vector = args.data[0];
	auto &target_resolution_vector = args.data[1];

	// Initial estimate; each cell expands to 4 children per resolution level
	ListVector::Reserve(result, args.size() * 4);
	uint64_t offset = 0;

	auto cell_list_data = FlatVector::GetData<uint64_t>(ListVector::GetEntry(cell_list_vector));

	auto result_size = ListVector::GetListSize(result);

	BinaryExecutor::Execute<list_entry_t, int32_t, list_entry_t>(
	    cell_list_vector, target_resolution_vector, result, args.size(),
	    [&](list_entry_t cell_list_entry, int32_t target_resolution) {
		    ValidateResolution(target_resolution, "a5_uncompact");
		    // We need to prepare the list of values to pass in.
		    auto compact_result =
		        a5_uncompact(cell_list_data + cell_list_entry.offset, cell_list_entry.length, target_resolution);
		    ThrowCellArrayError(compact_result, "a5_uncompact");
		    if (compact_result.len == 0) {
			    a5_free_cell_array(compact_result);
			    return list_entry_t {0, 0};
		    }
		    ListVector::Reserve(result, result_size + compact_result.len);
		    for (size_t i = 0; i < compact_result.len; i++) {
			    ListVector::PushBack(result, Value::UBIGINT(compact_result.data[i]));
		    }
		    result_size += compact_result.len;

		    list_entry_t out {offset, compact_result.len};
		    offset += compact_result.len;
		    a5_free_cell_array(compact_result);
		    return out;
	    });
}

static void LoadInternal(ExtensionLoader &loader) {
	// a5_cell_area: Returns the area of a cell at a given resolution
	{
		auto func = ScalarFunction("a5_cell_area", {LogicalType::INTEGER}, LogicalType::DOUBLE, A5CellAreaFun);
		CreateScalarFunctionInfo info(func);
		FunctionDescription desc;
		desc.description = "Returns the area in square meters of an A5 cell at the specified resolution level";
		desc.parameter_names = {"resolution"};
		desc.parameter_types = {LogicalType::INTEGER};
		desc.examples = {"a5_cell_area(10)"};
		desc.categories = {"a5", "geospatial"};
		info.descriptions.push_back(std::move(desc));
		loader.RegisterFunction(std::move(info));
	}

	// a5_get_num_cells: Returns the total number of cells at a given resolution
	{
		auto func = ScalarFunction("a5_get_num_cells", {LogicalType::INTEGER}, LogicalType::UBIGINT, A5GetNumCellsFun);
		CreateScalarFunctionInfo info(func);
		FunctionDescription desc;
		desc.description = "Returns the total number of A5 cells at the specified resolution level (0-30)";
		desc.parameter_names = {"resolution"};
		desc.parameter_types = {LogicalType::INTEGER};
		desc.examples = {"a5_get_num_cells(5)"};
		desc.categories = {"a5", "geospatial"};
		info.descriptions.push_back(std::move(desc));
		loader.RegisterFunction(std::move(info));
	}

	// a5_get_resolution: Returns the resolution of a cell
	{
		auto func = ScalarFunction("a5_get_resolution", {LogicalType::UBIGINT}, LogicalType::INTEGER, A5GetResolutionFun);
		CreateScalarFunctionInfo info(func);
		FunctionDescription desc;
		desc.description = "Returns the resolution level (0-30) of an A5 cell";
		desc.parameter_names = {"cell"};
		desc.parameter_types = {LogicalType::UBIGINT};
		desc.examples = {"a5_get_resolution(a5_lonlat_to_cell(-122.4, 37.8, 10))"};
		desc.categories = {"a5", "geospatial"};
		info.descriptions.push_back(std::move(desc));
		loader.RegisterFunction(std::move(info));
	}

	// a5_lonlat_to_cell: Converts longitude/latitude to a cell
	{
		auto func = ScalarFunction("a5_lonlat_to_cell", {LogicalType::DOUBLE, LogicalType::DOUBLE, LogicalType::INTEGER},
		                           LogicalType::UBIGINT, A5LonLatToCellFun);
		CreateScalarFunctionInfo info(func);
		FunctionDescription desc;
		desc.description = "Converts a longitude/latitude coordinate to an A5 cell at the specified resolution";
		desc.parameter_names = {"longitude", "latitude", "resolution"};
		desc.parameter_types = {LogicalType::DOUBLE, LogicalType::DOUBLE, LogicalType::INTEGER};
		desc.examples = {"a5_lonlat_to_cell(-122.4194, 37.7749, 10)"};
		desc.categories = {"a5", "geospatial"};
		info.descriptions.push_back(std::move(desc));
		loader.RegisterFunction(std::move(info));
	}

	// a5_cell_to_parent: Returns the parent cell at a given resolution
	{
		auto func = ScalarFunction("a5_cell_to_parent", {LogicalType::UBIGINT, LogicalType::INTEGER}, LogicalType::UBIGINT,
		                           A5CellToParentFun);
		CreateScalarFunctionInfo info(func);
		FunctionDescription desc;
		desc.description = "Returns the parent A5 cell at the specified coarser resolution";
		desc.parameter_names = {"cell", "parent_resolution"};
		desc.parameter_types = {LogicalType::UBIGINT, LogicalType::INTEGER};
		desc.examples = {"a5_cell_to_parent(a5_lonlat_to_cell(-122.4, 37.8, 10), 5)"};
		desc.categories = {"a5", "geospatial"};
		info.descriptions.push_back(std::move(desc));
		loader.RegisterFunction(std::move(info));
	}

	// a5_cell_to_lonlat: Returns the center longitude/latitude of a cell
	{
		auto func = ScalarFunction("a5_cell_to_lonlat", {LogicalType::UBIGINT}, LogicalType::ARRAY(LogicalType::DOUBLE, 2),
		                           A5CellToLonLatFun);
		CreateScalarFunctionInfo info(func);
		FunctionDescription desc;
		desc.description = "Returns the center point [longitude, latitude] of an A5 cell";
		desc.parameter_names = {"cell"};
		desc.parameter_types = {LogicalType::UBIGINT};
		desc.examples = {"a5_cell_to_lonlat(a5_lonlat_to_cell(-122.4, 37.8, 10))"};
		desc.categories = {"a5", "geospatial"};
		info.descriptions.push_back(std::move(desc));
		loader.RegisterFunction(std::move(info));
	}

	// a5_cell_to_children: Returns child cells
	{
		ScalarFunctionSet func_set("a5_cell_to_children");
		func_set.AddFunction(ScalarFunction("a5_cell_to_children", {LogicalType::UBIGINT, LogicalType::INTEGER},
		                                    LogicalType::LIST(LogicalType::UBIGINT), A5CellToChildrenFun));
		func_set.AddFunction(ScalarFunction("a5_cell_to_children", {LogicalType::UBIGINT},
		                                    LogicalType::LIST(LogicalType::UBIGINT), A5CellToChildrenFun));
		CreateScalarFunctionInfo info(func_set);

		// Description for two-argument variant
		FunctionDescription desc1;
		desc1.description = "Returns all child A5 cells at the specified finer resolution";
		desc1.parameter_names = {"cell", "child_resolution"};
		desc1.parameter_types = {LogicalType::UBIGINT, LogicalType::INTEGER};
		desc1.examples = {"a5_cell_to_children(a5_lonlat_to_cell(-122.4, 37.8, 5), 6)"};
		desc1.categories = {"a5", "geospatial"};
		info.descriptions.push_back(std::move(desc1));

		// Description for one-argument variant (immediate children)
		FunctionDescription desc2;
		desc2.description = "Returns the immediate child A5 cells (one resolution finer)";
		desc2.parameter_names = {"cell"};
		desc2.parameter_types = {LogicalType::UBIGINT};
		desc2.examples = {"a5_cell_to_children(a5_lonlat_to_cell(-122.4, 37.8, 5))"};
		desc2.categories = {"a5", "geospatial"};
		info.descriptions.push_back(std::move(desc2));

		loader.RegisterFunction(std::move(info));
	}

	// a5_get_res0_cells: Returns all resolution 0 cells
	{
		auto func = ScalarFunction("a5_get_res0_cells", {}, LogicalType::LIST(LogicalType::UBIGINT), A5GetRes0CellsFun);
		CreateScalarFunctionInfo info(func);
		FunctionDescription desc;
		desc.description = "Returns all 12 resolution 0 (root) A5 cells covering the entire globe";
		desc.parameter_names = {};
		desc.parameter_types = {};
		desc.examples = {"a5_get_res0_cells()"};
		desc.categories = {"a5", "geospatial"};
		info.descriptions.push_back(std::move(desc));
		loader.RegisterFunction(std::move(info));
	}

	// a5_cell_to_boundary: Returns the boundary polygon vertices
	{
		ScalarFunctionSet func_set("a5_cell_to_boundary");
		func_set.AddFunction(ScalarFunction({LogicalType::UBIGINT},
		                                    LogicalType::LIST(LogicalType::ARRAY(LogicalType::DOUBLE, 2)),
		                                    A5CellToBoundaryFun));
		func_set.AddFunction(ScalarFunction({LogicalType::UBIGINT, LogicalType::BOOLEAN},
		                                    LogicalType::LIST(LogicalType::ARRAY(LogicalType::DOUBLE, 2)),
		                                    A5CellToBoundaryFun));
		func_set.AddFunction(ScalarFunction({LogicalType::UBIGINT, LogicalType::BOOLEAN, LogicalType::INTEGER},
		                                    LogicalType::LIST(LogicalType::ARRAY(LogicalType::DOUBLE, 2)),
		                                    A5CellToBoundaryFun));
		CreateScalarFunctionInfo info(func_set);

		// Description for one-argument variant
		FunctionDescription desc1;
		desc1.description = "Returns the boundary vertices of an A5 cell as a closed ring of [lon, lat] points";
		desc1.parameter_names = {"cell"};
		desc1.parameter_types = {LogicalType::UBIGINT};
		desc1.examples = {"a5_cell_to_boundary(a5_lonlat_to_cell(-122.4, 37.8, 5))"};
		desc1.categories = {"a5", "geospatial"};
		info.descriptions.push_back(std::move(desc1));

		// Description for two-argument variant
		FunctionDescription desc2;
		desc2.description = "Returns the boundary vertices of an A5 cell, optionally as an open or closed ring";
		desc2.parameter_names = {"cell", "closed_ring"};
		desc2.parameter_types = {LogicalType::UBIGINT, LogicalType::BOOLEAN};
		desc2.examples = {"a5_cell_to_boundary(a5_lonlat_to_cell(-122.4, 37.8, 5), false)"};
		desc2.categories = {"a5", "geospatial"};
		info.descriptions.push_back(std::move(desc2));

		// Description for three-argument variant
		FunctionDescription desc3;
		desc3.description =
		    "Returns the boundary vertices of an A5 cell with configurable ring closure and edge interpolation segments";
		desc3.parameter_names = {"cell", "closed_ring", "segments"};
		desc3.parameter_types = {LogicalType::UBIGINT, LogicalType::BOOLEAN, LogicalType::INTEGER};
		desc3.examples = {"a5_cell_to_boundary(a5_lonlat_to_cell(-122.4, 37.8, 5), true, 4)"};
		desc3.categories = {"a5", "geospatial"};
		info.descriptions.push_back(std::move(desc3));

		loader.RegisterFunction(std::move(info));
	}

	// a5_compact: Compacts a set of cells
	{
		auto func = ScalarFunction("a5_compact", {LogicalType::LIST(LogicalType::UBIGINT)},
		                           LogicalType::LIST(LogicalType::UBIGINT), A5CompactFun);
		CreateScalarFunctionInfo info(func);
		FunctionDescription desc;
		desc.description = "Compacts a list of A5 cells by merging complete sets of sibling cells into parent cells";
		desc.parameter_names = {"cells"};
		desc.parameter_types = {LogicalType::LIST(LogicalType::UBIGINT)};
		desc.examples = {"a5_compact(a5_cell_to_children(a5_lonlat_to_cell(-122.4, 37.8, 5)))"};
		desc.categories = {"a5", "geospatial"};
		info.descriptions.push_back(std::move(desc));
		loader.RegisterFunction(std::move(info));
	}

	// a5_uncompact: Uncompacts cells to a target resolution
	{
		auto func = ScalarFunction("a5_uncompact", {LogicalType::LIST(LogicalType::UBIGINT), LogicalType::INTEGER},
		                           LogicalType::LIST(LogicalType::UBIGINT), A5UncompactFun);
		CreateScalarFunctionInfo info(func);
		FunctionDescription desc;
		desc.description = "Expands a compacted list of A5 cells to the specified target resolution";
		desc.parameter_names = {"cells", "target_resolution"};
		desc.parameter_types = {LogicalType::LIST(LogicalType::UBIGINT), LogicalType::INTEGER};
		desc.examples = {"a5_uncompact([a5_lonlat_to_cell(-122.4, 37.8, 5)], 7)"};
		desc.categories = {"a5", "geospatial"};
		info.descriptions.push_back(std::move(desc));
		loader.RegisterFunction(std::move(info));
	}

	QueryFarmSendTelemetry(loader, "a5", "2025110601");
}

void A5Extension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}
std::string A5Extension::Name() {
	return "a5";
}

std::string A5Extension::Version() const {
	return "2025110601";
}

} // namespace duckdb

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(a5, loader) {
	duckdb::LoadInternal(loader);
}
}
