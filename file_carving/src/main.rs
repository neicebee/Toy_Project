use std::{fs, io::{self, Read}, fmt::Write as FmtWrite};

// 파일 바이너리 읽기 함수
fn read_file_as_binary(file_path: &str) -> io::Result<Vec<u8>> {
    let mut file = fs::File::open(file_path)?;
    let mut buffer = Vec::new();
    file.read_to_end(&mut buffer)?;
    Ok(buffer)        
}

fn bmp_file_carving(source_byte: &[u8], output_log_file_path: &str, cluster_size: usize) -> io::Result<()> {
    let mut log_content = String::new();	// 로그 파일에 작성할 문자열
    let bmp_signature: [u8; 2] = [0x42, 0x4D];  // bmp 파일의 시그니처
    
    // 슬라이딩 윈도우를 이용한 시그니처 순회 검색
    for (i, window) in source_byte.windows(2).enumerate() {
        if window==bmp_signature {  // 1️⃣ 시그니처 일치 검증
            if (i+14)>source_byte.len() { continue; }	// 추출한 14바이트가 원본 파일의 최대 바이트를 넘어서는 경우 접근 제어
            let relevant_bytes = &source_byte[i..(i+14)];  // 슬라이싱
            // 2️⃣ bmp 파일의 예약 공간 바이트 검증
			// 배열은 '0-indexed'이기에 인덱스 6~9 확인
            if !(relevant_bytes[6]==0x00 && relevant_bytes[7]==0x00 &&
                relevant_bytes[8]==0x00 && relevant_bytes[9]==0x00) { continue; }
            // 3️⃣ bmp 파일 크기 정보 추출
            let file_size_bytes: [u8; 4] = [relevant_bytes[2], relevant_bytes[3], relevant_bytes[4], relevant_bytes[5],];
            let file_size = u32::from_le_bytes(file_size_bytes);    // 리틀 엔디언 방식의 값을 10진수 값으로 변환
            // 4️⃣ 클러스터 경계 정렬 검증
            let is_cluster_aligned = (i%cluster_size)==0;
            let cluster_alignment_info = if is_cluster_aligned { "Aligned" } else { "Not Aligned" };
            // 5️⃣ 파일 카빙 및 저장
            let end_offset_for_carving = (i+file_size as usize).min(source_byte.len());	// 원본 파일의 바이트를 넘어서는지 항상 확인
            let carved_data = &source_byte[i..end_offset_for_carving];    // 원본 바이트에서 bmp 파일에 해당하는 부분 슬라이싱
            // 추출 파일 길이가 헤더 명시 크기와 다른 경우 로그 남기기
            if carved_data.len()<file_size as usize {
                let _ = writeln!(log_content, "[WARN] Partial carve: Offset 0x{:08x}, Header size: {}, Actual carved size: {}, Cluster: {}",
                i, file_size, carved_data.len(), cluster_alignment_info);
            }
            let output_bmp_file_name = format!("result/{:08x}.bmp", i);    // 오프셋 기반의 파일명
            match fs::write(&output_bmp_file_name, carved_data) {
                Ok(_) => {
                    let _ = writeln!(log_content, "[SUCCESS] Carved BMP file: '{}', Offset: 0x{:08x}, Size: {}bytes, Carved Length: {}bytes, Cluster Alignment: {}",
                    output_bmp_file_name, i, file_size, carved_data.len(), cluster_alignment_info);
                },
                Err(e) => {
                    let _ = writeln!(log_content, "[ERROR] Failed to save '{}' (Offset: 0x{:08x}): {}, Cluster Alignment: {}",
                    output_bmp_file_name, i, e, cluster_alignment_info);
                }
            }
        }
    }
    fs::write(output_log_file_path, log_content)?;
    Ok(())
}

fn jpg_file_carving(source_byte: &[u8], output_log_file_path: &str, cluster_size: usize) -> io::Result<()> {
    let mut log_content = String::new();	// 로그 파일에 작성할 문자열
    let mut found_chunks_count = 0; // 찾은 청크 개수
    let jpg_soi: [u8; 4] = [0xFF, 0xD8, 0xFF, 0xE0];  // jpg 파일 SOI 및 App Marker
    let jpg_eoi: [u8; 4] = [0xFF, 0xD9, 0x00, 0x00];    // jpg 파일 EOI(램 슬랙 카빙)
    
    // 슬라이딩 윈도우를 이용한 SOI 및 EOI 탐색
    let _ = writeln!(log_content, "--- Finding SOI-EOI Chunks ---");
    for (i, window) in source_byte.windows(4).enumerate() {
        if window==jpg_soi {    // SOI 발견 시
            let soi_start_offset = i;   // SOI 오프셋 저장
            let mut eoi_end_offset: Option<usize> = None;   // EOI 오프셋용 변수 선언
            // 검색 범위 지정용 변수
            let search_start_index = soi_start_offset+jpg_soi.len();    // SOI 값 이후로 검색 시작 지점 지정
            let search_end_index = source_byte.len().saturating_sub(jpg_eoi.len()-1);   // 원본 데이터를 벗어나지 않도록 검색 끝 지점 지정
            // saturating_sub => 결과값이 음수가 되지 않도록 포화시킴
            
            // 검색 범위 내에서 EOI 검색
            for j in search_start_index..search_end_index {
                let current_four_bytes = &source_byte[j..(j+jpg_eoi.len())];
                if current_four_bytes==jpg_eoi {    // EOI 발견 시
                    eoi_end_offset = Some(j);   // EOI 시작 오프셋 저장
                    break;
                }
            }
            // jpg 파일 카빙
            if let Some(eoi_offset) = eoi_end_offset {
                let estimated_jpg_length = (eoi_offset+jpg_eoi[0..2].len())-soi_start_offset;   // 유추 jpg 파일의 크기는 EOI(0xffd9)까지
                let required_clusters = (estimated_jpg_length as f64/cluster_size as f64).ceil() as usize;  // 요구되는 클러스터 크기(개수)
                let restore_file_size = required_clusters*cluster_size; // 실제로 jpg가 차지하는 용량(슬랙 공간 포함)
                let carve_end_offset = (soi_start_offset+restore_file_size).min(source_byte.len()); // SOI 오프셋부터 실제 jpg 차지 용량까지 카빙(원본 데이터를 넘지 않게)
                let restored_data = &source_byte[soi_start_offset..carve_end_offset];   // 카빙 용량을 기반으로 데이터 저장
                let output_jpg_file_name = format!("result/{:08x}.jpg", soi_start_offset);  // 오프셋 기반의 파일명
                match fs::write(&output_jpg_file_name, &restored_data) {
                    Ok(_) => {
                        found_chunks_count+=1;
                        let _ = writeln!(log_content, "CHUNK {} - SOI at 0x{:08x} -> EOI at 0x{:08x}, JPG Length: {}bytes, Required Clusters: {}, Restored to: '{}' (Size: {}bytes)",
                        found_chunks_count, soi_start_offset, eoi_offset, estimated_jpg_length, required_clusters, output_jpg_file_name, restored_data.len());
                    },
                    Err(e) => {
                        let _ = writeln!(log_content, "[ERROR] Failed to save restored JPG '{}' (Offset: 0x{:08x}): {}",
                        output_jpg_file_name, soi_start_offset, e);
                    }
                }
            } else {
                let _ = writeln!(log_content, "[UNPAIRED] SOI found at 0x{:08x}, but no subsequent EOI (0xFFD9) found within the remaining data.", soi_start_offset);
            }
        }
    }
    let _ = writeln!(log_content, "\nTotal: {}", found_chunks_count);
    fs::write(output_log_file_path, log_content)?;
    Ok(())
}

fn main() {
    // const BMP_CLUSTER_SIZE: usize = 4096;
    // let bmp_carving_file_path = "carving_file/dd_bmp_carving_1.001";
    // let output_log_file_path = "result/carved_bmp_log.txt";
    
    // // 바이너리 값 읽어오기
    // let bytes = match read_file_as_binary(bmp_carving_file_path) {
    //     Ok(data) => data,
    //     Err(e) => { eprintln!("Error: {e}"); return; }
    // };
    
	// // bmp 카빙
    // match bmp_file_carving(&bytes, output_log_file_path, BMP_CLUSTER_SIZE) {
    //     Ok(_) => { println!("Success!"); },
    //     Err(e) => { eprintln!("Error: {e}"); }
    // }
    
    const JPG_CLUSTER_SIZE: usize = 2048;
    let jpg_carving_file_path = "carving_file/dd_jpg_carving_1.001";
    let output_log_file_path = "result/carved_jpg_log.txt";
    
    // 바이너리 값 읽어오기
    let bytes = match read_file_as_binary(jpg_carving_file_path) {
        Ok(data) => data,
        Err(e) => { eprintln!("Error: {e}"); return; }
    };
    
    match jpg_file_carving(&bytes, output_log_file_path, JPG_CLUSTER_SIZE) {
        Ok(_) => { println!("Success!"); },
        Err(e) => { eprintln!("Error: {e}"); }
    }
}