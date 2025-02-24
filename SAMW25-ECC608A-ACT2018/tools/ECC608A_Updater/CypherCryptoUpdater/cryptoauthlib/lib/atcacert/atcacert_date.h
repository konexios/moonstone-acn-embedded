/**
 * \file
 * \brief Declarations for date handling with regard to certificates.
 *
 * Copyright (c) 2015 Atmel Corporation. All rights reserved.
 *
 * \atmel_crypto_device_library_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel integrated circuit.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \atmel_crypto_device_library_license_stop
 */

#ifndef ATCACERT_DATE_H
#define ATCACERT_DATE_H

#include "atcacert.h"

/** \defgroup atcacert_ Certificate manipulation methods (atcacert_)
 *
 * \brief
 * These methods provide convenient ways to perform certification I/O with
 * CryptoAuth chips and perform certificate manipulation in memory
 *
   @{ */

/**
 * Holds a broken-down date in UTC. Mimics atcacert_tm_utc_t from time.h.
 */
typedef struct atcacert_tm_utc_s {
	int tm_sec;  // 0 to 59
	int tm_min;  // 0 to 59
	int tm_hour; // 0 to 23
	int tm_mday; // 1 to 31
	int tm_mon;  // 0 to 11
	int tm_year; // years since 1900
} atcacert_tm_utc_t;

/**
 * Date formats.
 */
typedef enum atcacert_date_format_e {
	DATEFMT_ISO8601_SEP,     //!< ISO8601 full date YYYY-MM-DDThh:mm:ssZ
	DATEFMT_RFC5280_UTC,     //!< RFC 5280 (X.509) 4.1.2.5.1 UTCTime format YYMMDDhhmmssZ
	DATEFMT_POSIX_UINT32_BE, //!< POSIX (aka UNIX) date format. Seconds since Jan 1, 1970. 32 bit unsigned integer, big
	                         //!< endian.
	DATEFMT_POSIX_UINT32_LE, //!< POSIX (aka UNIX) date format. Seconds since Jan 1, 1970. 32 bit unsigned integer,
	                         //!< little endian.
	DATEFMT_RFC5280_GEN      //!< RFC 5280 (X.509) 4.1.2.5.2 GeneralizedTime format YYYYMMDDhhmmssZ
} atcacert_date_format_t;

#define DATEFMT_ISO8601_SEP_SIZE (20)
#define DATEFMT_RFC5280_UTC_SIZE (13)
#define DATEFMT_POSIX_UINT32_BE_SIZE (4)
#define DATEFMT_POSIX_UINT32_LE_SIZE (4)
#define DATEFMT_RFC5280_GEN_SIZE (15)
#define DATEFMT_MAX_SIZE DATEFMT_ISO8601_SEP_SIZE

static const size_t ATCACERT_DATE_FORMAT_SIZES[] = {DATEFMT_ISO8601_SEP_SIZE,
                                                    DATEFMT_RFC5280_UTC_SIZE,
                                                    DATEFMT_POSIX_UINT32_BE_SIZE,
                                                    DATEFMT_POSIX_UINT32_LE_SIZE,
                                                    DATEFMT_RFC5280_GEN_SIZE};

// Inform function naming when compiling in C++
#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Format a timestamp according to the format type.
 *
 * \param[in]    format               Format to use.
 * \param[in]    timestamp            Timestamp to format.
 * \param[out]   formatted_date       Formatted date will be return in this buffer.
 * \param[inout] formatted_date_size  As input, the size of the formatted_date buffer.
 *                                    As output, the size of the returned formatted_date.
 *
 * \return 0 on success
 */
int atcacert_date_enc(atcacert_date_format_t format, const atcacert_tm_utc_t *timestamp, uint8_t *formatted_date,
                      size_t *formatted_date_size);

/**
 * \brief Parse a formatted timestamp according to the specified format.
 *
 * \param[in]  format               Format to parse the formatted date as.
 * \param[in]  formatted_date       Formatted date to be parsed.
 * \param[in]  formatted_date_size  Size of the formatted date in bytes.
 * \param[out] timestamp            Parsed timestamp is returned here.
 *
 * \return 0 on success
 */
int atcacert_date_dec(atcacert_date_format_t format, const uint8_t *formatted_date, size_t formatted_date_size,
                      atcacert_tm_utc_t *timestamp);

/**
 * \brief Encode the issue and expire dates in the format used by the compressed certificate.
 *
 * \param[in]  issue_date    Issue date to encode. Note that minutes and seconds will be ignored.
 * \param[in]  expire_years  Expire date is expressed as a number of years past the issue date.
 *                           0 should be used if there is no expire date.
 * \param[out] enc_dates     Encoded dates for use in the compressed certificate is returned here.
 *                           3 bytes.
 *
 * \return 0 on success
 */
int atcacert_date_enc_compcert(const atcacert_tm_utc_t *issue_date, uint8_t expire_years, uint8_t enc_dates[3]);

/**
 * \brief Decode the issue and expire dates from the format used by the compressed certificate.
 *
 * \param[in]  enc_dates           Encoded date from the compressed certificate. 3 bytes.
 * \param[in]  expire_date_format  Expire date format. Only used to determine max date when no
 *                                 expiration date is specified by the encoded date.
 * \param[out] issue_date          Decoded issue date is returned here.
 * \param[out] expire_date         Decoded expire date is returned here. If there is no
 *                                 expiration date, the expire date will be set to a maximum
 *                                 value for the given expire_date_format.
 *
 * \return 0 on success
 */
int atcacert_date_dec_compcert(const uint8_t enc_dates[3], atcacert_date_format_t expire_date_format,
                               atcacert_tm_utc_t *issue_date, atcacert_tm_utc_t *expire_date);

/**
 * \brief Return the maximum date available for the given format.
 *
 * \param[in]  format     Format to get the max date for.
 * \param[out] timestamp  Max date is returned here.
 *
 * \return 0 on success
 */
int atcacert_date_get_max_date(atcacert_date_format_t format, atcacert_tm_utc_t *timestamp);

int atcacert_date_enc_iso8601_sep(const atcacert_tm_utc_t *timestamp, uint8_t formatted_date[DATEFMT_ISO8601_SEP_SIZE]);

int atcacert_date_dec_iso8601_sep(const uint8_t formatted_date[DATEFMT_ISO8601_SEP_SIZE], atcacert_tm_utc_t *timestamp);

int atcacert_date_enc_rfc5280_utc(const atcacert_tm_utc_t *timestamp, uint8_t formatted_date[DATEFMT_RFC5280_UTC_SIZE]);

int atcacert_date_dec_rfc5280_utc(const uint8_t formatted_date[DATEFMT_RFC5280_UTC_SIZE], atcacert_tm_utc_t *timestamp);

int atcacert_date_enc_rfc5280_gen(const atcacert_tm_utc_t *timestamp, uint8_t formatted_date[DATEFMT_RFC5280_GEN_SIZE]);

int atcacert_date_dec_rfc5280_gen(const uint8_t formatted_date[DATEFMT_RFC5280_GEN_SIZE], atcacert_tm_utc_t *timestamp);

int atcacert_date_enc_posix_uint32_be(const atcacert_tm_utc_t *timestamp,
                                      uint8_t                  formatted_date[DATEFMT_POSIX_UINT32_BE_SIZE]);

int atcacert_date_dec_posix_uint32_be(const uint8_t      formatted_date[DATEFMT_POSIX_UINT32_BE_SIZE],
                                      atcacert_tm_utc_t *timestamp);

int atcacert_date_enc_posix_uint32_le(const atcacert_tm_utc_t *timestamp,
                                      uint8_t                  formatted_date[DATEFMT_POSIX_UINT32_LE_SIZE]);

int atcacert_date_dec_posix_uint32_le(const uint8_t      formatted_date[DATEFMT_POSIX_UINT32_LE_SIZE],
                                      atcacert_tm_utc_t *timestamp);

/** @} */
#ifdef __cplusplus
}
#endif

#endif
