#!/bin/bash
top=$1
sw_dir=$top/sw
src_dir=${sw_dir}/linuxsrc

build_dir=${src_dir}/build
pkg_dir=${src_dir}/pkg
image_dir=${src_dir}/images/pb47

BUILD_WLAN="rel dbg"
CHIPS_WLAN="magpie_gmac"

BUILD_WLAN_CMN="common_adf pb47_tool"

PKG_SDK_PRE_WLAN="linux wpa2 magpie_gmac_tgt"
PKG_SDK_POST_WLAN="magpie_gmac_hst"
PKG_IMAGES="magpie_gmac"

func_loop()
{
    val=""
	for val in $2
	do
        $1 $val
	done	
}
clean_wlan()
{
    cd ${build_dir}

	echo "Cleaning the targets"
	
	make aed_magpie_clean BOARD_TYPE=pb47 BUILD_TYPE=jffs2
}

package_sdk()
{
    cd ${build_dir}

	echo "Packaging $1 SDK complete"
	make pkg_${1}_sdk BOARD_TYPE=pb47 BUILD_TYPE=jffs2
}

build_common()
{
    cd ${build_dir}


    echo "Building $1 build components..."
	make $1_build BOARD_TYPE=pb47 BUILD_TYPE=jffs2 
}

build_wlan_driver()
{
    cd ${build_dir}
    
    chip=""
    pkg=""

	for chip in ${CHIPS_WLAN}
	do
		echo "Building $chip Specific components..."

        BUILD_TARGETS=pkg_build_${chip}_$1
		
        clean_wlan
		
        make ${BUILD_TARGETS/_rel/} BOARD_TYPE=pb47 BUILD_TYPE=jffs2
	done
	
	make jffs2_build BOARD_TYPE=pb47 BUILD_TYPE=jffs2
	
	echo "Building the gmac $1 images"
	
    IMAGE_TARGETS=pkg_${PKG_IMAGES}_$1_images

	make ${IMAGE_TARGETS/_rel/} BOARD_TYPE=pb47 BUILD_TYPE=jffs2

    for pkg in ${PKG_SDK_POST_WLAN}
    do
        PACKAGE_TARGETS=${pkg}_${1}

        package_sdk ${PACKAGE_TARGETS/_rel/}
    done
}


#########################################################
###################### MAIN FUNCTION ####################
#########################################################


#Packaging the generic source components
func_loop "package_sdk" "${PKG_SDK_PRE_WLAN}"

#Build Common Components
func_loop "build_common" "${BUILD_WLAN_CMN}"

#Build WLAN drivers for various builds
func_loop "build_wlan_driver" "${BUILD_WLAN}"

cd ${src_dir}
cp -fv ${pkg_dir}/* ${image_dir}/


