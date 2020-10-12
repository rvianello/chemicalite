macro(chemicalite_pytest module)
    add_test(${module}
                ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/${module}.py)
endmacro(chemicalite_pytest)

chemicalite_pytest(load_extension)
chemicalite_pytest(molecule)
chemicalite_pytest(descriptors)
chemicalite_pytest(bitstring)
chemicalite_pytest(rdtree)

