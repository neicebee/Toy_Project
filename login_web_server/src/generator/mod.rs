use rand::seq::IteratorRandom;  // 슬라이스에서 랜덤하게 요소 선택 import
use rand::Rng; // 난수 생성 트레이트 import

const GROUP_SIZE: usize = 6; // C 코드의 GROUP_SIZE
const NUM_GROUPS: usize = 3; // C 코드처럼 3개의 그룹

const LOWERCASE: &str = "abcdefghijklmnopqrstuvwxyz";
const UPPERCASE: &str = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const DIGITS: &str = "0123456789";

// size_t random_index(size_t max) { return arc4random_uniform((uint32_t)max); }ndex(max: usize) -> usize {
// fn random_index(max: usize) -> usize {
//     // 현재 스레드의 난수 생성기 획득 후 0 ~ max-1 까지의 범위에서 난수 생성
//     rand::thread_rng().gen_range(0..max)
// }

// char get_random_char(const char *charset, size_t len) { return charset[random_index(len)]; }
fn get_random_char(charset: &str) -> char {
    // choose() 메서드로 charset 슬라이스 중 랜덤하게 하나 선택(Chars Iterator는 슬라이스와 유사)
    charset.chars().choose(&mut rand::thread_rng()).unwrap()
}

// int is_consecutive(char a, char b) { ... }
fn is_consecutive(a: char, b: char) -> bool {
    // 소문자 변환
    let a_lower = a.to_lowercase().next().unwrap_or(a);
    let b_lower = b.to_lowercase().next().unwrap_or(b);
    
    // 알파벳 확인
    if !a_lower.is_ascii_lowercase()||!b_lower.is_ascii_lowercase() { return false; }
    
    // 두 문자 연속되거나 같은 알파벳인지 확인(절댓값 차이<=1)
    // as u8로 ascii 값을 가져와서 차이 계산
    (a_lower as i32 - b_lower as i32).abs()<=1
}

// void generate_virtual_word(char *group) { ... }
fn generate_virtual_word(group: &mut Vec<char>) {   // Vec<char>를 가변 참조로 받아 수정
    let mut i = 0;
    while i<GROUP_SIZE {
        if group.get(i).is_some()&&group[i]!='\0' { // group.get(i).is_some(): 인덱스가 유효한지 확인. group[i]!= \0': 초기화된 문자가 아닌지 확인.
             i += 1; continue; // 숫자나 대문자가 이미 있으면 건너뛰기
        }
        let c = get_random_char(LOWERCASE);
        // i>0&&is_consecutive(group[i-1], c)
        if i>0&&group.get(i-1).is_some() && is_consecutive(group[i-1], c) { // 이전 문자가 있는지 확인 후 연속 검사
            continue; // 연속된 알파벳 방지
        }
        // group[i++] = c; // C 스타일. Rust에서는 push 또는 인덱스 접근
        if let Some(char_ref) = group.get_mut(i) { // 가변 참조를 가져와서 값 변경
             *char_ref = c;
        } else {
            // 벡터 길이가 GROUP_SIZE 보다 작다면 push
            group.push(c);
        }
        i+=1; // 인덱스 증가
    }
}

// void generate_password(char *password) { ... }
// 반환 타입은 String
pub fn generate_password() -> String {
    // 동적 메모리 할당 방식 (Rust에서는 Vec<Vec<char>> 사용)
    let mut groups: Vec<Vec<char>> = (0..NUM_GROUPS).map(|_| vec!['\0'; GROUP_SIZE]).collect(); // NUM_GROUPS 개수의 Vec<char> (GROUP_SIZE 길이) 생성, '\0'으로 초기화

    // 숫자 및 대문자 삽입 시 중복 충돌 방지 로직
    let mut rng = rand::thread_rng(); // 난수 생성기 다시 가져오기
    let digit_group = rng.gen_range(0..NUM_GROUPS);
    let mut upper_group;
    loop { // do-while 대신 loop 사용
        upper_group = rng.gen_range(0..NUM_GROUPS);
        if upper_group!=digit_group { break; }
    }
    let digit_pos = rng.gen_range(0..GROUP_SIZE);
    let mut upper_pos;
    loop { // do-while 대신 loop 사용
        upper_pos = rng.gen_range(0..GROUP_SIZE);
        if upper_pos!=digit_pos { break; }
    }

    // 숫자 및 대문자 삽입
    groups[digit_group][digit_pos] = get_random_char(DIGITS);
    groups[upper_group][upper_pos] = get_random_char(UPPERCASE);

    // 가상 단어 생성
    for i in 0..NUM_GROUPS { // 0부터 NUM_GROUPS-1 까지 반복
        generate_virtual_word(&mut groups[i]);
    }

    // 최종 암호 형식으로 재조합
    // snprintf(password, PASSWORD_LENGTH+1, "%s-%s-%s", groups[0], groups[1], groups[2]);
    // format! 매크로 사용
    // Vec<char>를 String으로 변환 (collect() 사용)
    let password_string = format!(
        "{}",
        groups.into_iter().map(|g| g.into_iter().collect::<String>()).collect::<Vec<String>>().join("-") // 각 Vec<char>를 String으로 만들고 "-"로 연결
        // C 코드는 %s-%s-%s 로 3개 그룹만 출력하는데, 실제 패스워드 길이는 20글자.
        // GROUP_SIZE가 6이면 6*3 + 2('-' 2개) = 20 글자가 딱 맞음.
        // C 코드의 snprintf 포맷은 groups[0], groups[1], groups[2] 세 개만 사용하므로,
        // Rust 코드에서도 groups[0], groups[1], groups[2]만 사용해야 함.
    );
    // groups 변수를 into_iter().collect::<Vec<String>>() 해서 소유권이 이동했으므로 더 이상 groups 사용 불가.

    // C 코드에서는 패스워드 길이 제한이 있었음 (PASSWORD_LENGTH)
    // Rust 코드에서는 String이 동적이므로 길이 제한은 필요에 따라 추가 구현.
    // 현재 로직대로면 6-6-6 형태로 총 20자리가 생성됨.

    // Rust에서는 메모리 해제(free)가 자동 관리되므로 필요 없음.
    password_string
}