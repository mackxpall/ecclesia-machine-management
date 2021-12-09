"""Starlark definitions for build rules embed data files directly into code libraries."""

load(":file.bzl", "expand_template")

def cc_data_library(
        name,
        data,
        cc_namespace,
        var_name,
        visibility = None,
        flatten = False):
    """Define a data library containing a set of embedded files.

    A cc_library will be generated by this rule that exports an array containing
    embedded data from all of the given data files. This library will have a
    header named ${name}.h and a source file named ${name}.cc.

    By default all the data files will be embedded using their path relative to
    the package name. If the flatten argument is true then their names will be
    reduced to their base names.

    Args:
      name: The name of the cc_library that will contain the embedded data.
      data: A list of data files to embed into the cc_library.
      cc_namespace: The C++ namespace to wrap the generated variable into.
      var_name: The name of the C++ variable used to access the data.
      visibility: The visibility of the generated cc_library.
      flatten: If true, embed all the data file names using just their basename.
    """

    # Wrap the data files to embed in a filegroup. We do this to avoid issues with
    # filenames that would otherwise get interpreted as Make variables (e.g. $metadata).
    # Including those files directly in the genrule srcs and using $(SRCS) would have
    # that problem, but routing them through a filegroup avoids that problem.
    native.filegroup(
        name = name + "__data_filegroup",
        srcs = data,
    )

    # Run the cc_embed tool to generate a header and code file embedding all the data
    # files. Note that we strip three prefixes from the file paths: first the BINDIR and
    # GENDIR which ensures that every file is represented with its path relative to the
    # workspace root, and second the package name to change the path to being relative
    # to the local package instead of the global workspace path. This stripping is
    # redundant in cases where we specify --flatten but for simplicity we just always
    # pass in the stripping args.
    flatten_arg = "--flatten" if flatten else ""
    native.genrule(
        name = name + "__generator",
        srcs = [":" + name + "__data_filegroup"],
        outs = [name + ".cc", name + ".h"],
        cmd = ("$(location //ecclesia/lib/file:cc_embed) " +
               "--output_name=%s --output_dir=\"$(@D)\" --namespace=%s --variable_name=%s " +
               "--strip_prefixes=$(BINDIR),$(GENDIR),%s %s " +
               "$(locations :%s__data_filegroup)") %
              (name, cc_namespace, var_name, native.package_name(), flatten_arg, name),
        exec_tools = ["//ecclesia/lib/file:cc_embed"],
    )

    # Encapsulate the code an header files into a cc_library.
    native.cc_library(
        name = name,
        srcs = [name + ".cc"],
        hdrs = [name + ".h"],
        deps = ["//ecclesia/lib/file:cc_embed_interface"],
        visibility = visibility,
    )

def shar_binary(
        name,
        src,
        script = "",
        shell = "/bin/bash",
        data = [],
        visibility = None):
    """Define a shell archive binary which bundles a shell script into a binary.

    This library will produce as output a binary which when executed will unpack the
    contents of the binary (a shell script and one or more data files) to a temporary
    directory and then execute the shell script using the specified shell.

    The script will be executed with an environment variable of ECCLESIA_DATA_DIR
    pointing at the temporary directory where the script was unpacked.

    Note that this rule does not bundle in the shell itself and so the produced
    binary is not hermetic. It depends on the shell in question being present on
    the system where the binary is executed.

    Args:
      name: The name of the cc_binary that this will produce.
      src: The shell script to be executed. (e.g. "//my_pack:my_target")
      script: The data path of the shell script to be executed. (e.g. "my_target.sh").
        If script is left empty, the 'src' parameter will be used as the data path.
      shell: The full (absolute) path of the shell that should execute the script.
      data: Data files to be included in the binary. Note that the shell script
        will be added to this script and so you cannot include a data file with
        the same path as the shell script.
      visibility: The visibility of the generated cc_binary.
    """

    # Pack all of the provided data files into a data library, along with the shell
    # script to execute. This effectively embeds the entire set of files into the
    # resulting binary.
    cc_data_library(
        name = name + "__shar_data",
        cc_namespace = "ecclesia",
        data = [src] + data,
        var_name = "kSharDataFiles",
    )

    # Generate a C++ stub using the shar_stub.cc template. Using a template to generate
    # code is not ideal but there's no way for us to use a fixed path for the header for
    # the embedded data and so we need a template to inject a dynamic header path.
    #
    # We could avoid having to inject the shell an script paths into the template by
    # instead embedding them into the data library but since we need to use the template
    # anyway for the header path it's simpler to inject the paths as well.
    expand_template(
        name = name + "__stub",
        out = name + "__stub.cc",
        substitutions = {
            "$DATA_HEADER": native.package_name() + "/" + name + "__shar_data.h",
            "$SHELL": shell,
            "$SCRIPT": script if script else src,
        },
        template = "//ecclesia/build_defs:shar_stub.cc.template",
    )

    # Combine the stub and data library into an actual binary.
    native.cc_binary(
        name = name,
        srcs = [name + "__stub.cc"],
        deps = [
            ":" + name + "__shar_data",
            "@com_google_absl//absl/strings",
            "//ecclesia/lib/file:dir",
            "//ecclesia/lib/file:path",
            "//ecclesia/lib/logging",
            "//ecclesia/lib/logging:posix",
        ],
        visibility = visibility,
    )
