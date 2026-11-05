//! Only public wallpaper catalog data crosses into the C9 web playback app.
use crate::{model::Preferences, video_catalog};
use serde_json::{Value, json};

pub const APP_ID: &str = "com.airecken.luma.wallpaper";

pub fn launch_params(preferences: &Preferences) -> Option<Value> {
    let catalog = video_catalog::all();
    let selected = video_catalog::selected(&preferences.video_excluded);
    if selected.is_empty() {
        return None;
    }
    let index = selected
        .iter()
        .position(|&index| index == preferences.video_index)
        .unwrap_or(0);
    let clips: Vec<Value> = selected
        .iter()
        .map(|&index| json!({"url": catalog[index].url, "title": catalog[index].name}))
        .collect();
    Some(json!({"clips": clips, "index": index, "shuffle": preferences.video_shuffle}))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn only_selected_catalog_videos_are_shared() {
        let catalog = video_catalog::all();
        let mut preferences = Preferences {
            video_index: 2,
            video_excluded: catalog
                .iter()
                .enumerate()
                .filter(|(index, _)| *index != 2 && *index != 5)
                .map(|(_, video)| video.id.clone())
                .collect(),
            ..Default::default()
        };
        preferences.greeting_name = "PRIVATE_NAME".into();
        let params = launch_params(&preferences).unwrap();
        assert_eq!(params["clips"].as_array().unwrap().len(), 2);
        assert_eq!(params["index"], 0);
        assert_eq!(params["clips"][0]["url"], catalog[2].url);
        assert!(!params.to_string().contains("PRIVATE_NAME"));
        preferences.video_excluded = catalog.iter().map(|video| video.id.clone()).collect();
        assert!(launch_params(&preferences).is_none());
    }
}
