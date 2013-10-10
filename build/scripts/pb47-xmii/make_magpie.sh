#!/bin/bash
top=$1
sw_dir=$top/sw
src_dir=${sw_dir}/linuxsrc

build_dir=${src_dir}/build
pkg_dir=${top}/pkg/pb47/src
image_dir=${src_dir}/images/pb47
scripts_dir=${src_dir}/build/scripts/pb47
LSDK=lsdk_newma_xmii
LSDK_WLAN=${LSDK}_wlan
LSDK_FW=${LSDK}_fw

package_items()
{
    cd ${src_dir}

    case $1 in
        "lsdk")
        tar -czvf ${pkg_dir}/${LSDK}.tgz  \
            -T ${scripts_dir}/LSDK.include  \
            -X ${scripts_dir}/LSDK.exclude
        ;;

        "fw")
        tar -czvf ${pkg_dir}/${LSDK_FW}.tgz \
            -T ${scripts_dir}/LSDK-FW.include \
            -X ${scripts_dir}/LSDK-FW.exclude
        ;;

        "wlan")
        tar -czvf ${pkg_dir}/${LSDK_WLAN}.tgz  \
            -T ${scripts_dir}/LSDK-WLAN.include \
            -X ${scripts_dir}/LSDK-WLAN.exclude
        ;;

        "prepare")
        mkdir -p ${pkg_dir}
        ;;

      esac  
}

build ()
{
    cd ${build_dir}

    case $1 in
        "driver")
        make BOARD_TYPE=pb47 BUILD_TYPE=jffs2 driver_build
        ;;
        *)
        make BOARD_TYPE=pb47 BUILD_TYPE=jffs2 
        ;;

    esac
}
clean()
{
    cd ${build_dir}

    case $1 in
        "wlan")
        make BOARD_TYPE=pb47 BUILD_TYPE=jffs2 pb47_clean
        ;;

        *)
        make BOARD_TYPE=pb47 BUILD_TYPE=jffs2 clean
        ;;

    esac
}
#########################################################
###################### MAIN FUNCTION ####################
#########################################################

#############
## package ##
#############
package_items "prepare"
package_items "lsdk"
package_items "fw"

#############
## build   ##
#############
clean "all"
build "all"
clean "wlan"

#############
## package ##
#############
package_items "wlan"
