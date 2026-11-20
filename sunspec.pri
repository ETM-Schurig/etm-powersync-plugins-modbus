QT *= network serialbus

# libnymea-sunspec is NOT vendored: we link against the
# libnymea-sunspec-dev package. Two possible resolutions, same convention as
# modbus.pri / NYMEA_MODBUS_PATH:
#
#   1) system-installed package -> pkg-config nymea-sunspec (default)
#   2) local sysroot            -> qmake6 NYMEA_SUNSPEC_PATH=/path/to/sysroot
#      where <sysroot> contains usr/include/nymea-sunspec and usr/lib/<triplet>.
#
# The 1.15.2 package installs the model headers under
#   <includedir>/home/jenkins/nymea-plugins-modbus/libnymea-sunspec/models/
# (the Jenkins build path leaked into the .deb). The plugin does
#   #include <models/sunspecmodelfactory.h>
# which therefore does not resolve with -I<includedir> alone. We add the missing
# prefix when it exists, without breaking a correct installation.

isEmpty(NYMEA_SUNSPEC_PATH) {
    CONFIG += link_pkgconfig
    PKGCONFIG += nymea-sunspec
    SUNSPEC_INCLUDEDIR = $$[QT_INSTALL_PREFIX]/include/nymea-sunspec
} else {
    SUNSPEC_INCLUDEDIR = $$NYMEA_SUNSPEC_PATH/usr/include/nymea-sunspec
    SUNSPEC_LIBDIR = $$NYMEA_SUNSPEC_PATH/usr/lib/$$QMAKE_HOST.arch-linux-gnu
    INCLUDEPATH += $$SUNSPEC_INCLUDEDIR
    LIBS += -L$$SUNSPEC_LIBDIR -lnymea-sunspec1
    # Development build against a sysroot: the library is not in the
    # dynamic linker cache, so it has to be embedded. This
    # rpath does not exist in the package build, which goes through pkg-config.
    QMAKE_RPATHDIR += $$SUNSPEC_LIBDIR
}

SUNSPEC_MODELS_WORKAROUND = $$SUNSPEC_INCLUDEDIR/home/jenkins/nymea-plugins-modbus/libnymea-sunspec
exists($$SUNSPEC_MODELS_WORKAROUND/models): INCLUDEPATH += $$SUNSPEC_MODELS_WORKAROUND
