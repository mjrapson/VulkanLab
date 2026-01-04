add_library(pch STATIC
    pch.cpp
)

target_link_libraries(pch PRIVATE)

target_precompile_headers(pch PUBLIC
    <algorithm>
    <array>
    <cassert>
    <chrono>
    <filesystem>
    <fstream>
    <functional>
    <memory>
    <optional>
    <ranges>
    <span>
    <stdexcept>
    <stdint.h>
    <string>
    <tuple>
    <unordered_map>
    <utility>
    <vector>
)
