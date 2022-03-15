//% weight=111 color=#A88002 icon="\uf030" block="VIA Pixetto" 
namespace pixetto {

	//% block="initialize vision sensor RX %rx TX %tx"
	//% shim=pixetto::begin
	//% weight=100
	export function begin(rx: PixSerialPin, tx: PixSerialPin): boolean {
		return false
    }
    
    //% block="get firmware version"
    //% shim=pixetto::pxtGetVersion
    //% weight=95
    export function pxtGetVersion(): number {
		return 0;
    }
    
	//% block="set function %func"
	//% shim=pixetto::pxtSetFunc
	//% weight=94
	export function pxtSetFunc(func: PixFunction): void {
		return;
    }
    
    //% block="get detection result number"
    //% shim=pixetto::pxtAvailable
    //% weight=93
    export function pxtAvailable(): number {
		return 0;
    }
    
    //% block="get one detection result"
    //% shim=pixetto::pxtGetData
    //% weight=92
    export function pxtGetData(): boolean {
		return false;
    }
    
    //% block="get %field"
    //% shim=pixetto::getDataField
    //% weight=90
    export function getDataField(field: PixDataField): number {
        return 0;
    }
}
