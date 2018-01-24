project('listbox', 'c')

gtk_dep = dependency('gtk+-4.0', version: '>= 3.89')


sources = files([
  'src/gd-model-list-box.c',
])

headers = files([
  'src/gd-model-list-box.h',
])

liblistbox = library(
  'listbox',
  sources,
  dependencies: gtk_dep
)

# Reference this from a superproject
liblistbox_dep = declare_dependency(
  link_with: liblistbox,
  dependencies: gtk_dep,
  include_directories: include_directories('src/')
)

subdir('tests')


# (Maybe) build demos
if get_option('demos')
  demo_deps = [gtk_dep, liblistbox_dep]

  imagelist_demo = executable(
    'imagelist',
    'demos/imagelist.c',
    dependencies: demo_deps,
  )

  demo_demo = executable(
    'demo',
    'demos/demo.c',
    dependencies: demo_deps
  )

endif
