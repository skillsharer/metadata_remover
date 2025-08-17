include(BundleUtilities)

# Fix up a CLI-style bundle: copy all dependent .dylib files next to the exe
# and rewrite install names so it runs standalone from the folder.
function(fixup_cli_bundle exe_path)
  set(BU_CHMOD_BUNDLE_ITEMS ON)
  set(BU_COPY_FULL_FRAMEWORK_CONTENTS ON)

  # We let fixup_bundle discover transitive deps of the executable
  # and copy them alongside the executable.
  # Note: By default, fixup_bundle wants a "bundle" dir, but for CLI
  # we can pass the exe path directly.
  fixup_bundle("${exe_path}" "" "${CMAKE_INSTALL_PREFIX}")

  # At this point, Exiv2 + its deps (zlib, expat, etc.) should be copied
  # right next to 'cleanmeta' and their install names adjusted to @rpath/@loader_path.
  # If you prefer placing dylibs under ./lib, you can move them and run install_name_tool,
  # but keeping them beside the exe is simplest and robust.
endfunction()
