# Maintainer: Fabrice Corraire <antidote1911@gmail.com>

pkgname=arsenic
pkgver=2.7
pkgrel=1
pkgdesc="Qt5 app for encrypt files and text with triple encryption"
arch=('x86_64')
url="https://github.com/Antidote1911/${pkgname}"
license=('GPL')
depends=('qt5-base')
source=("${pkgname}-v${pkgver}.tar.gz::https://github.com/Antidote1911/${pkgname}/archive/v${pkgver}/${pkgname}-v${pkgver}.tar.gz")
sha256sums=('0D444DCF6B6242A5A1CDB5097B70CD426E032F7E5F6199CD75C6E7F93072154B')

build() {
    cd "${srcdir}/${pkgname}-${pkgver}"
    qmake-qt5 QMAKE_DEFAULT_INCDIRS="" ${pkgname}.pro
    make
}

package() {
    cd "${srcdir}/${pkgname}-${pkgver}"    
    make INSTALL_ROOT="${pkgdir}/" install
}
