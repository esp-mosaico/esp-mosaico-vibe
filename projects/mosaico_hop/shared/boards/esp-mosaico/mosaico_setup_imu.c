// SPDX-License-Identifier: Apache-2.0
/* BMI270 uses the Board Manager I2C bus directly; no second bus owner. */
#include "mosaico_hardware.h"
#include <stdlib.h>
#include <string.h>
#include "bmi270.h"
#include "esp_board_manager_includes.h"
#include "esp_rom_sys.h"
#include "gen_board_device_custom.h"

#define IMU_BUS "i2c_master"
#define IMU_TRANSFER_TIMEOUT_MS 100
#define IMU_TRANSFER_BYTES 32U

typedef struct {
    struct bmi2_dev sensor;
    i2c_master_dev_handle_t device;
    bool bus_referenced;
} mosaico_imu_t;

static BMI2_INTF_RETURN_TYPE imu_read(uint8_t reg, uint8_t *data,
                                     uint32_t count, void *ctx)
{
    mosaico_imu_t *imu = ctx;
    return i2c_master_transmit_receive(imu->device, &reg, 1, data, count,
        IMU_TRANSFER_TIMEOUT_MS) == ESP_OK ? BMI2_INTF_RET_SUCCESS : -1;
}

static BMI2_INTF_RETURN_TYPE imu_write(uint8_t reg, const uint8_t *data,
                                      uint32_t count, void *ctx)
{
    mosaico_imu_t *imu = ctx;
    if (count > IMU_TRANSFER_BYTES) return -1;
    uint8_t bytes[IMU_TRANSFER_BYTES + 1] = {reg};
    memcpy(bytes + 1, data, count);
    return i2c_master_transmit(imu->device, bytes, count + 1,
        IMU_TRANSFER_TIMEOUT_MS) == ESP_OK ? BMI2_INTF_RET_SUCCESS : -1;
}

static void imu_delay(uint32_t us, void *ctx)
{
    (void)ctx;
    esp_rom_delay_us(us);
}

int mosaico_hardware_imu_deinit(void *handle)
{
    mosaico_imu_t *imu = handle;
    if (!imu) return ESP_ERR_INVALID_ARG;
    if (imu->device) {
        esp_err_t err = i2c_master_bus_rm_device(imu->device);
        if (err != ESP_OK) return err;
        imu->device = NULL;
    }
    if (imu->bus_referenced) {
        esp_err_t err = esp_board_periph_unref_handle(IMU_BUS);
        if (err != ESP_OK) return err;
        imu->bus_referenced = false;
    }
    free(imu);
    return ESP_OK;
}

static int imu_sensor_init(void *config, int size, void **out)
{
    if (!config || !out || size != sizeof(dev_custom_imu_sensor_config_t))
        return ESP_ERR_INVALID_ARG;
    *out = NULL;
    const dev_custom_imu_sensor_config_t *active = config;
    mosaico_imu_t *imu = calloc(1, sizeof(*imu));
    if (!imu) return ESP_ERR_NO_MEM;
    i2c_master_bus_handle_t bus = NULL;
    esp_err_t err = esp_board_periph_ref_handle(IMU_BUS, (void **)&bus);
    if (err != ESP_OK) {
        free(imu);
        return err;
    }
    imu->bus_referenced = true;
    i2c_device_config_t device = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = active->addr,
        .scl_speed_hz = 400000,
    };
    /* Both strap addresses occur on Mosaico boards. Probe before the BMI270
     * configuration upload rather than retrying a partially configured sensor. */
    if (i2c_master_probe(bus, device.device_address, IMU_TRANSFER_TIMEOUT_MS) != ESP_OK) {
        device.device_address = device.device_address == 0x69 ? 0x68 : 0x69;
    }
    err = i2c_master_bus_add_device(bus, &device, &imu->device);
    if (err != ESP_OK) goto failed;
    imu->sensor = (struct bmi2_dev) {
        .intf = BMI2_I2C_INTF, .intf_ptr = imu,
        .read = imu_read, .write = imu_write, .delay_us = imu_delay,
        .read_write_len = IMU_TRANSFER_BYTES,
    };
    if (bmi270_init(&imu->sensor) != BMI2_OK) {
        err = ESP_FAIL;
        goto failed;
    }
    struct bmi2_sens_config sensors[2] = {{.type = BMI2_ACCEL}, {.type = BMI2_GYRO}};
    if (bmi2_get_sensor_config(sensors, 2, &imu->sensor) != BMI2_OK) {
        err = ESP_FAIL;
        goto failed;
    }
    sensors[0].cfg.acc.odr = BMI2_ACC_ODR_100HZ;
    sensors[0].cfg.acc.range = BMI2_ACC_RANGE_4G;
    sensors[0].cfg.acc.bwp = BMI2_ACC_NORMAL_AVG4;
    sensors[0].cfg.acc.filter_perf = BMI2_PERF_OPT_MODE;
    sensors[1].cfg.gyr.odr = BMI2_GYR_ODR_100HZ;
    sensors[1].cfg.gyr.range = BMI2_GYR_RANGE_2000;
    sensors[1].cfg.gyr.bwp = BMI2_GYR_NORMAL_MODE;
    sensors[1].cfg.gyr.noise_perf = BMI2_POWER_OPT_MODE;
    sensors[1].cfg.gyr.filter_perf = BMI2_PERF_OPT_MODE;
    uint8_t enabled[] = {BMI2_ACCEL, BMI2_GYRO};
    if (bmi2_set_sensor_config(sensors, 2, &imu->sensor) != BMI2_OK ||
            bmi2_sensor_enable(enabled, 2, &imu->sensor) != BMI2_OK) {
        err = ESP_FAIL;
        goto failed;
    }
    *out = imu;
    return ESP_OK;
failed:
    (void)mosaico_hardware_imu_deinit(imu);
    return err;
}

esp_err_t mosaico_hardware_imu_read(void *handle, float *x, float *y, float *z)
{
    mosaico_imu_t *imu = handle;
    if (!imu || !x || !y || !z) return ESP_ERR_INVALID_ARG;
    struct bmi2_sens_data data = {0};
    if (bmi2_get_sensor_data(&data, &imu->sensor) != BMI2_OK) return ESP_FAIL;
    const float scale = 4.0f / 32768.0f;
    *x = data.acc.x * scale;
    *y = data.acc.y * scale;
    *z = data.acc.z * scale;
    return ESP_OK;
}

CUSTOM_DEVICE_IMPLEMENT(imu_sensor, imu_sensor_init, mosaico_hardware_imu_deinit);
