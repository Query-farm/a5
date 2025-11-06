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

inline void A5CellAreaFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &resolution_vector = args.data[0];
	UnaryExecutor::Execute<int32_t, double>(resolution_vector, result, args.size(), [&](int32_t resolution) {
		if (resolution > MAX_RESOLUTION) {
			throw InvalidInputException("Resolution must be between 0 and 30");
		}
		return a5_cell_area(resolution);
	});
}

inline void A5GetNumCellsFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &resolution_vector = args.data[0];
	UnaryExecutor::Execute<int32_t, uint64_t>(resolution_vector, result, args.size(), [&](int32_t resolution) {
		if (resolution > MAX_RESOLUTION) {
			throw InvalidInputException("Resolution must be between 0 and 30");
		}
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
		    if (resolution > MAX_RESOLUTION) {
			    throw InvalidInputException("Resolution must be between 0 and 30");
		    }
		    struct ResultU64 res = a5_lon_lat_to_cell(lon, lat, resolution);
		    if (res.error != nullptr) {
			    throw InvalidInputException(res.error);
		    }
		    return res.value;
	    });
}

inline void A5CellToParentFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &cell_vector = args.data[0];
	auto &parent_resolution_vector = args.data[1];

	BinaryExecutor::Execute<uint64_t, int32_t, uint64_t>(
	    cell_vector, parent_resolution_vector, result, args.size(), [&](uint64_t cell, int32_t parent_resolution) {
		    if (parent_resolution > MAX_RESOLUTION) {
			    throw InvalidInputException("Resolution must be between 0 and 30");
		    }
		    struct ResultU64 res = a5_cell_to_parent(cell, parent_resolution);
		    if (res.error != nullptr) {
			    throw InvalidInputException(res.error);
		    }
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
		if (res.error != nullptr) {
			throw InvalidInputException(res.error);
		}

		data_ptr[i * 2] = res.longitude;
		data_ptr[i * 2 + 1] = res.latitude;
	}

	if (args.size() == 1) {
		result.SetVectorType(VectorType::CONSTANT_VECTOR);
	}
}

inline void A5CellToChildrenFun(DataChunk &args, ExpressionState &state, Vector &result) {
	ListVector::Reserve(result, args.size() * 4);
	uint64_t offset = 0;

	if (args.ColumnCount() == 2) {
		auto &cell_vector = args.data[0];
		auto &max_resolution_vector = args.data[1];

		BinaryExecutor::Execute<uint64_t, int32_t, list_entry_t>(
		    cell_vector, max_resolution_vector, result, args.size(), [&](uint64_t cell_id, int32_t child_resolution) {
			    auto child_result = a5_cell_to_children(cell_id, child_resolution);

			    if (child_result.error) {
				    throw InvalidInputException(child_result.error);
			    }

			    if (child_result.len == 0) {
				    return list_entry_t {0, 0};
			    }
			    for (auto i = 0; i < child_result.len; i++) {
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

			if (child_result.error) {
				throw InvalidInputException(child_result.error);
			}

			if (child_result.len == 0) {
				return list_entry_t {0, 0};
			}
			for (auto i = 0; i < child_result.len; i++) {
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
		if (boundary_result.error) {
			throw InvalidInputException(boundary_result.error);
		}
		if (boundary_result.len == 0) {
			a5_free_lonlatdegrees_array(boundary_result);
			return {0, 0};
		}

		for (int i = 0; i < boundary_result.len; i++) {
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
	for (auto i = 0; i < cells.len; i++) {
		cell_vec.emplace_back(Value::UBIGINT(cells.data[i]));
	}
	a5_free_cell_array(cells);
	Value val = Value::LIST(LogicalType::UBIGINT, cell_vec);

	D_ASSERT(args.ColumnCount() == 0);
	result.Reference(val);
}

inline void A5CompactFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &cell_list_vector = args.data[0];

	ListVector::Reserve(result, args.size() * 4);
	uint64_t offset = 0;

	auto cell_list_data = FlatVector::GetData<uint64_t>(ListVector::GetEntry(cell_list_vector));

	auto result_size = ListVector::GetListSize(result);

	UnaryExecutor::Execute<list_entry_t, list_entry_t>(
	    cell_list_vector, result, args.size(), [&](list_entry_t cell_list_entry) {
		    // We need to prepare the list of values to pass in.
		    auto compact_result = a5_compact(cell_list_data + cell_list_entry.offset, cell_list_entry.length);
		    if (compact_result.error) {
			    throw InvalidInputException(compact_result.error);
		    }
		    if (compact_result.len == 0) {
			    return list_entry_t {0, 0};
		    }
		    ListVector::Reserve(result, result_size + compact_result.len);
		    for (auto i = 0; i < compact_result.len; i++) {
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

	ListVector::Reserve(result, args.size() * 4);
	uint64_t offset = 0;

	auto cell_list_data = FlatVector::GetData<uint64_t>(ListVector::GetEntry(cell_list_vector));

	auto result_size = ListVector::GetListSize(result);

	BinaryExecutor::Execute<list_entry_t, int32_t, list_entry_t>(
	    cell_list_vector, target_resolution_vector, result, args.size(),
	    [&](list_entry_t cell_list_entry, int32_t target_resolution) {
		    // We need to prepare the list of values to pass in.
		    auto compact_result =
		        a5_uncompact(cell_list_data + cell_list_entry.offset, cell_list_entry.length, target_resolution);
		    if (compact_result.error) {
			    throw InvalidInputException(compact_result.error);
		    }
		    if (compact_result.len == 0) {
			    return list_entry_t {0, 0};
		    }
		    ListVector::Reserve(result, result_size + compact_result.len);
		    for (auto i = 0; i < compact_result.len; i++) {
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
	auto a5_cell_area_func = ScalarFunction("a5_cell_area", {LogicalType::INTEGER}, LogicalType::DOUBLE, A5CellAreaFun);
	loader.RegisterFunction(a5_cell_area_func);

	auto a5_get_num_cells_func =
	    ScalarFunction("a5_get_num_cells", {LogicalType::INTEGER}, LogicalType::UBIGINT, A5GetNumCellsFun);
	loader.RegisterFunction(a5_get_num_cells_func);

	auto a5_get_resolution_func =
	    ScalarFunction("a5_get_resolution", {LogicalType::UBIGINT}, LogicalType::INTEGER, A5GetResolutionFun);
	loader.RegisterFunction(a5_get_resolution_func);

	auto a5_lon_lat_to_cell_func =
	    ScalarFunction("a5_lonlat_to_cell", {LogicalType::DOUBLE, LogicalType::DOUBLE, LogicalType::INTEGER},
	                   LogicalType::UBIGINT, A5LonLatToCellFun);
	loader.RegisterFunction(a5_lon_lat_to_cell_func);

	auto a5_cell_to_parent_func = ScalarFunction("a5_cell_to_parent", {LogicalType::UBIGINT, LogicalType::INTEGER},
	                                             LogicalType::UBIGINT, A5CellToParentFun);
	loader.RegisterFunction(a5_cell_to_parent_func);

	auto a5_cell_to_lon_lat_func = ScalarFunction("a5_cell_to_lonlat", {LogicalType::UBIGINT},
	                                              LogicalType::ARRAY(LogicalType::DOUBLE, 2), A5CellToLonLatFun);
	loader.RegisterFunction(a5_cell_to_lon_lat_func);

	auto a5_cell_to_children_set = ScalarFunctionSet("a5_cell_to_children");
	a5_cell_to_children_set.AddFunction(ScalarFunction("a5_cell_to_children",
	                                                   {LogicalType::UBIGINT, LogicalType::INTEGER},
	                                                   LogicalType::LIST(LogicalType::UBIGINT), A5CellToChildrenFun));
	a5_cell_to_children_set.AddFunction(ScalarFunction("a5_cell_to_children", {LogicalType::UBIGINT},
	                                                   LogicalType::LIST(LogicalType::UBIGINT), A5CellToChildrenFun));
	loader.RegisterFunction(a5_cell_to_children_set);

	auto a5_get_res0_cells_func =
	    ScalarFunction("a5_get_res0_cells", {}, LogicalType::LIST(LogicalType::UBIGINT), A5GetRes0CellsFun);
	loader.RegisterFunction(a5_get_res0_cells_func);

	auto boundary_set = ScalarFunctionSet("a5_cell_to_boundary");
	boundary_set.AddFunction(ScalarFunction(
	    {LogicalType::UBIGINT}, LogicalType::LIST(LogicalType::ARRAY(LogicalType::DOUBLE, 2)), A5CellToBoundaryFun));
	boundary_set.AddFunction(ScalarFunction({LogicalType::UBIGINT, LogicalType::BOOLEAN},
	                                        LogicalType::LIST(LogicalType::ARRAY(LogicalType::DOUBLE, 2)),
	                                        A5CellToBoundaryFun));
	boundary_set.AddFunction(ScalarFunction({LogicalType::UBIGINT, LogicalType::BOOLEAN, LogicalType::INTEGER},
	                                        LogicalType::LIST(LogicalType::ARRAY(LogicalType::DOUBLE, 2)),
	                                        A5CellToBoundaryFun));
	loader.RegisterFunction(boundary_set);

	auto a5_cell_to_boundary_func =
	    ScalarFunction("a5_cell_to_boundary", {LogicalType::UBIGINT, LogicalType::BOOLEAN, LogicalType::INTEGER},
	                   LogicalType::LIST(LogicalType::ARRAY(LogicalType::DOUBLE, 2)), A5CellToBoundaryFun);
	loader.RegisterFunction(a5_cell_to_boundary_func);

	auto a5_compact_func = ScalarFunction("a5_compact", {LogicalType::LIST(LogicalType::UBIGINT)},
	                                      LogicalType::LIST(LogicalType::UBIGINT), A5CompactFun);
	loader.RegisterFunction(a5_compact_func);

	auto a5_uncompact_func =
	    ScalarFunction("a5_uncompact", {LogicalType::LIST(LogicalType::UBIGINT), LogicalType::INTEGER},
	                   LogicalType::LIST(LogicalType::UBIGINT), A5UncompactFun);
	loader.RegisterFunction(a5_uncompact_func);

	QueryFarmSendTelemetry(loader, "a5", "2025101601");
}

void A5Extension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}
std::string A5Extension::Name() {
	return "a5";
}

std::string A5Extension::Version() const {
	return "2025101601";
}

} // namespace duckdb

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(a5, loader) {
	duckdb::LoadInternal(loader);
}
}
