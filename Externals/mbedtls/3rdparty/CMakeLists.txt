list (APPEND thirdparty_src)
list (APPEND thirdparty_lib)
list (APPEND thirdparty_inc_public)
list (APPEND thirdparty_inc)
list (APPEND thirdparty_def)

add_subdirectory(everest)

set(thirdparty_src ${thirdparty_src} PARENT_SCOPE)
set(thirdparty_lib ${thirdparty_lib} PARENT_SCOPE)
set(thirdparty_inc_public ${thirdparty_inc_public} PARENT_SCOPE)
set(thirdparty_inc ${thirdparty_inc} PARENT_SCOPE)
set(thirdparty_def ${thirdparty_def} PARENT_SCOPE)
