macro(chemicalite_test module)
    add_test(${module} 
             python ${CMAKE_CURRENT_SOURCE_DIR}/${module}.py 
	     ${CHEMICALITE_LOCATION})
endmacro(chemicalite_test)

chemicalite_test(load_extension)
chemicalite_test(molecule)
chemicalite_test(descriptors)
