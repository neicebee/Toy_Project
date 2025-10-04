use std::{fs, io::{self, Read}, fmt::Write as FmtWrite};

// 파일 바이너리 읽기 함수
fn read_file_as_binary(file_path: &str) -> io::Result<Vec<u8>> {
    let mut file = fs::File::open(file_path)?;
    let mut buffer = Vec::new();
    file.read_to_end(&mut buffer)?;
    Ok(buffer)        
}

fn main() {
    const BMP_CLUSTER_SIZE: usize = 4096;
    let bmp_carving_file = "carving_file/dd_bmp_carving.001";
    let bmp_dump_file = "result/bmp_hex_dump.txt";
    
    // 바이너리 값 읽어오기
    let bytes = match read_file_as_binary(bmp_carving_file) {
        Ok(data) => data,
        Err(e) => { eprintln!("Error: {e}"); return; }
    };
    
    let mut content = String::new();
    let bmp_signature: [u8; 2] = [0x42, 0x4D];  // bmp 파일의 시그니처
    
    // 슬라이딩 윈도우를 이용한 시그니처 순회 검색
    for (i, window) in bytes.windows(2).enumerate() {
        if window==bmp_signature {  // 시그니처 일치 검증
            if (i+14)>bytes.len() { continue; }
            let relevant_bytes = &bytes[i..(i+14)];  // 슬라이싱
            // bmp 파일의 예약 공간 바이트 검증
            if !(relevant_bytes[6]==0x00 && relevant_bytes[7]==0x00 &&
                relevant_bytes[8]==0x00 && relevant_bytes[9]==0x00) { continue; }
            // bmp 파일 크기 정보 추출
            let file_size_bytes: [u8; 4] = [relevant_bytes[2], relevant_bytes[3], relevant_bytes[4], relevant_bytes[5],];
            let file_size = u32::from_le_bytes(file_size_bytes);    // 리틀 엔디언 방식의 값을 10진수 값으로 변환
            // 클러스터 경계 정렬 검증
            let is_cluster_aligned = (i%BMP_CLUSTER_SIZE)==0;
            let cluster_alignment_info = if is_cluster_aligned { "Aligned" } else { "Not Aligned" };
            // 파일 카빙 및 저장
            let end_offset_for_carving = (i+file_size as usize).min(bytes.len());
            let carved_data = &bytes[i..end_offset_for_carving];    // 원본 바이트에서 bmp 파일에 해당하는 부분 추출
            // 추출 파일 길이가 헤더 명시 크기와 다른 경우 로그 남기기
            if carved_data.len()<file_size as usize {
                let _ = writeln!(content, "WARN] Partial carve: Offset 0x{:08x}, Header size: {}, Actual carved size: {}, Cluster: {}",
                i, file_size, carved_data.len(), cluster_alignment_info);
            }
            let output_bmp_file_name = format!("result/{:08x}.bmp", i);    // 오프셋 기반의 파일명
            match fs::write(&output_bmp_file_name, carved_data) {
                Ok(_) => {
                    let _ = writeln!(content, "[SUCCESS] Carved BMP file: '{}', Offset: 0x{:08x}, Size: {}bytes, Carved Length: {}bytes, Cluster Alignment: {}",
                    output_bmp_file_name, i, file_size, carved_data.len(), cluster_alignment_info);
                },
                Err(e) => {
                    let _ = writeln!(content, "[ERROR] Failed to save '{}' (Offset: 0x{:08x}): {}, Cluster Alignment: {}",
                    output_bmp_file_name, i, e, cluster_alignment_info);
                }
            }
        }
    }
    
    match fs::write(bmp_dump_file, content) {
        Ok(_) => { println!("Success!"); },
        Err(e) => { eprintln!("Error: {e}"); }
    }
}