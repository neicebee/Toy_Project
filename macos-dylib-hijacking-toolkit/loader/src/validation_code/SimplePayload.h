/**
 * SimplePayload.h
 * 
 * Type B 검증용 예제 Framework
 * TestApp이 사용하는 간단한 Framework
 */

#ifndef SIMPLE_PAYLOAD_H
#define SIMPLE_PAYLOAD_H

/**
 * 정상 Framework 함수
 */
void simple_payload_init(void);
void simple_payload_process(const char *data);
const char* simple_payload_version(void);

/**
 * Constructor/Destructor
 */
__attribute__((constructor)) void simple_payload_constructor(void);
__attribute__((destructor)) void simple_payload_destructor(void);

#endif // SIMPLE_PAYLOAD_H
