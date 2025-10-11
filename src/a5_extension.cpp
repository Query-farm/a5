#define DUCKDB_EXTENSION_MAIN

#include "a5_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/function/scalar_function.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>
#include "rust.h"
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

static void LoadInternal(ExtensionLoader &loader) {

	auto a5_cell_area_func = ScalarFunction("a5_cell_area", {LogicalType::INTEGER}, LogicalType::DOUBLE, A5CellAreaFun);
	loader.RegisterFunction(a5_cell_area_func);

	auto a5_get_num_cells_func =
	    ScalarFunction("a5_num_cells", {LogicalType::INTEGER}, LogicalType::UBIGINT, A5GetNumCellsFun);
	loader.RegisterFunction(a5_get_num_cells_func);

	auto a5_get_resolution_func =
	    ScalarFunction("a5_resolution", {LogicalType::UBIGINT}, LogicalType::INTEGER, A5GetResolutionFun);
	loader.RegisterFunction(a5_get_resolution_func);

	auto a5_lon_lat_to_cell_func =
	    ScalarFunction("a5_lon_lat_to_cell", {LogicalType::DOUBLE, LogicalType::DOUBLE, LogicalType::INTEGER},
	                   LogicalType::UBIGINT, A5LonLatToCellFun);
	loader.RegisterFunction(a5_lon_lat_to_cell_func);

	auto a5_cell_to_parent_func = ScalarFunction("a5_cell_to_parent", {LogicalType::UBIGINT, LogicalType::INTEGER},
	                                             LogicalType::UBIGINT, A5CellToParentFun);
	loader.RegisterFunction(a5_cell_to_parent_func);

	auto a5_cell_to_lon_lat_func = ScalarFunction("a5_cell_to_lon_lat", {LogicalType::UBIGINT},
	                                              LogicalType::ARRAY(LogicalType::DOUBLE, 2), A5CellToLonLatFun);
	loader.RegisterFunction(a5_cell_to_lon_lat_func);
}

void A5Extension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}
std::string A5Extension::Name() {
	return "a5";
}

std::string A5Extension::Version() const {
	return "2025101101";
}

} // namespace duckdb

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(a5, loader) {
	duckdb::LoadInternal(loader);
}
}
